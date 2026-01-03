#include "image_decoder_worker.h"

#include <chrono>

#include "error_builder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

namespace webcodecs {

// =============================================================================
// CUSTOM AVIO CALLBACKS FOR MEMORY READING
// =============================================================================

namespace {

struct IOContext {
  const uint8_t* data;
  size_t size;
  size_t position;
};

int ReadPacket(void* opaque, uint8_t* buf, int buf_size) {
  auto* ctx = static_cast<IOContext*>(opaque);
  if (ctx->position >= ctx->size) {
    return AVERROR_EOF;
  }

  size_t remaining = ctx->size - ctx->position;
  size_t to_read = std::min(static_cast<size_t>(buf_size), remaining);

  std::memcpy(buf, ctx->data + ctx->position, to_read);
  ctx->position += to_read;

  return static_cast<int>(to_read);
}

int64_t SeekPacket(void* opaque, int64_t offset, int whence) {
  auto* ctx = static_cast<IOContext*>(opaque);

  switch (whence) {
    case SEEK_SET:
      ctx->position = static_cast<size_t>(offset);
      break;
    case SEEK_CUR:
      ctx->position += static_cast<size_t>(offset);
      break;
    case SEEK_END:
      ctx->position = ctx->size + static_cast<size_t>(offset);
      break;
    case AVSEEK_SIZE:
      return static_cast<int64_t>(ctx->size);
    default:
      return AVERROR(EINVAL);
  }

  if (ctx->position > ctx->size) {
    ctx->position = ctx->size;
  }

  return static_cast<int64_t>(ctx->position);
}

}  // namespace

// =============================================================================
// CUSTOM IO DELETER
// =============================================================================

void ImageDecoderWorker::CustomIODeleter::operator()(AVIOContext* ctx) const noexcept {
  if (ctx) {
    // Delete the opaque IOContext
    auto* io_data = static_cast<IOContext*>(ctx->opaque);
    delete io_data;
    // Free the buffer and context
    av_freep(&ctx->buffer);
    avio_context_free(&ctx);
  }
}

// =============================================================================
// CONSTRUCTOR / DESTRUCTOR
// =============================================================================

ImageDecoderWorker::ImageDecoderWorker(ImageControlQueue& queue)
    : queue_(queue) {}

ImageDecoderWorker::~ImageDecoderWorker() {
  Stop();
}

// =============================================================================
// LIFECYCLE
// =============================================================================

bool ImageDecoderWorker::Start() {
  std::lock_guard<std::mutex> lock(lifecycle_mutex_);

  if (running_.load(std::memory_order_acquire)) {
    return true;  // Already running
  }

  should_exit_.store(false, std::memory_order_release);

  try {
    worker_thread_ = std::thread(&ImageDecoderWorker::WorkerLoop, this);
    running_.store(true, std::memory_order_release);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

void ImageDecoderWorker::Stop() {
  std::lock_guard<std::mutex> lock(lifecycle_mutex_);

  if (!running_.load(std::memory_order_acquire)) {
    return;  // Not running
  }

  // Signal worker to exit
  should_exit_.store(true, std::memory_order_release);

  // Shutdown the queue to unblock any waiting Dequeue()
  queue_.Shutdown();

  // Join worker thread
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }

  running_.store(false, std::memory_order_release);
}

bool ImageDecoderWorker::IsRunning() const {
  return running_.load(std::memory_order_acquire);
}

bool ImageDecoderWorker::ShouldExit() const {
  return should_exit_.load(std::memory_order_acquire);
}

// =============================================================================
// WORKER LOOP
// =============================================================================

void ImageDecoderWorker::WorkerLoop() {
  while (!ShouldExit()) {
    // Dequeue with timeout to check for shutdown periodically
    auto msg_opt = queue_.DequeueFor(std::chrono::milliseconds(100));

    if (!msg_opt) {
      continue;  // Timeout or shutdown
    }

    ImageMessage& msg = *msg_opt;

    // Dispatch based on message type
    std::visit(
        MessageVisitor{
            [this](ImageConfigureMessage& m) {
              bool success = OnConfigure(m);
              if (!success) {
                // Error already signaled
              }
            },
            [this](ImageDecodeMessage& m) {
              OnDecode(m);
            },
            [this](ImageResetMessage&) {
              OnReset();
            },
            [this](ImageCloseMessage&) {
              OnClose();
              should_exit_.store(true, std::memory_order_release);
            },
            [this](ImageUpdateTrackMessage& m) {
              OnUpdateTrack(m);
            },
            [this](ImageStreamDataMessage& m) {
              OnStreamData(m);
            },
            [this](ImageStreamEndMessage&) {
              OnStreamEnd();
            },
            [this](ImageStreamErrorMessage& m) {
              OnStreamError(m);
            },
        },
        msg);
  }
}

// =============================================================================
// MESSAGE HANDLERS
// =============================================================================

bool ImageDecoderWorker::OnConfigure(const ImageConfigureMessage& msg) {
  // Store configuration
  type_ = msg.type;
  desired_width_ = msg.desired_width;
  desired_height_ = msg.desired_height;
  is_streaming_ = msg.is_streaming;
  prefer_animation_ = msg.prefer_animation;
  color_space_conversion_ = msg.color_space_conversion;
  stream_complete_ = false;
  configured_ = false;

  // For streaming mode, we wait for data to arrive via ImageStreamDataMessage
  if (is_streaming_) {
    image_data_.clear();
    read_position_ = 0;
    // Don't try to configure yet - wait for enough data
    return true;
  }

  // Non-streaming mode: copy image data for memory-based I/O
  image_data_ = msg.data;
  read_position_ = 0;

  if (image_data_.empty()) {
    OutputError(0, AVERROR_INVALIDDATA, "Empty image data");
    return false;
  }

  return TryConfigureFromBuffer();
}

bool ImageDecoderWorker::TryConfigureFromBuffer() {
  if (configured_) {
    return true;  // Already configured
  }

  if (image_data_.empty()) {
    return false;  // No data yet
  }

  // Create custom I/O context for reading from memory
  // We need to keep the IOContext alive, so we'll allocate it and store
  // ownership in the AVIOContext opaque field
  auto* io_ctx_data = new IOContext{image_data_.data(), image_data_.size(), 0};

  constexpr int kBufferSize = 32768;  // 32KB buffer
  auto* avio_buffer = static_cast<uint8_t*>(av_malloc(kBufferSize));
  if (!avio_buffer) {
    delete io_ctx_data;
    OutputError(0, AVERROR(ENOMEM), "Failed to allocate I/O buffer");
    return false;
  }

  AVIOContext* avio_ctx = avio_alloc_context(
      avio_buffer, kBufferSize,
      0,  // write_flag = 0 (read-only)
      io_ctx_data, ReadPacket, nullptr, SeekPacket);

  if (!avio_ctx) {
    av_free(avio_buffer);
    delete io_ctx_data;
    OutputError(0, AVERROR(ENOMEM), "Failed to allocate AVIO context");
    return false;
  }

  io_ctx_ = CustomAVIOContextPtr(avio_ctx);

  // Allocate format context
  AVFormatContext* fmt_ctx = avformat_alloc_context();
  if (!fmt_ctx) {
    OutputError(0, AVERROR(ENOMEM), "Failed to allocate format context");
    return false;
  }

  fmt_ctx->pb = io_ctx_.get();
  fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

  // Open input (probe format)
  // NOTE: avformat_open_input() frees fmt_ctx on failure, so don't double-free
  int ret = avformat_open_input(&fmt_ctx, nullptr, nullptr, nullptr);
  if (ret < 0) {
    OutputError(0, ret, "Failed to open image: " + errors::FfmpegErrorString(ret));
    return false;
  }

  fmt_ctx_ = raii::MakeAvFormatContext(fmt_ctx);

  // Find stream info
  ret = avformat_find_stream_info(fmt_ctx_.get(), nullptr);
  if (ret < 0) {
    OutputError(0, ret, "Failed to find stream info: " + errors::FfmpegErrorString(ret));
    return false;
  }

  // Build track list from video streams
  tracks_.clear();
  int32_t best_stream = -1;
  int best_score = -1;

  for (unsigned int i = 0; i < fmt_ctx_->nb_streams; ++i) {
    AVStream* stream = fmt_ctx_->streams[i];
    if (stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
      continue;
    }

    ImageTrackInfo track;
    track.stream_index = static_cast<int>(i);

    // Check if animated (GIF, APNG, WebP animation)
    int64_t nb_frames = stream->nb_frames;
    if (nb_frames <= 0) {
      // Try to get frame count from codec-specific metadata
      nb_frames = 1;
    }

    track.frame_count = static_cast<uint32_t>(std::max(nb_frames, int64_t{1}));
    track.animated = (track.frame_count > 1);

    // Get loop count from metadata (for GIF)
    const AVDictionaryEntry* loop = av_dict_get(stream->metadata, "loop_count", nullptr, 0);
    if (loop) {
      track.repetition_count = static_cast<float>(std::atof(loop->value));
    } else {
      // Infinite loop by default for animated images
      track.repetition_count = track.animated ? -1.0f : 0.0f;
    }

    tracks_.push_back(track);

    // Score this stream for selection
    int score = 0;
    if (prefer_animation_.has_value()) {
      if (prefer_animation_.value() && track.animated) {
        score += 100;
      } else if (!prefer_animation_.value() && !track.animated) {
        score += 100;
      }
    }
    score += stream->codecpar->width * stream->codecpar->height;  // Prefer larger

    if (score > best_score) {
      best_score = score;
      best_stream = static_cast<int32_t>(tracks_.size() - 1);
    }
  }

  if (tracks_.empty()) {
    OutputError(0, AVERROR_INVALIDDATA, "No video streams found in image");
    return false;
  }

  // Select best stream
  if (best_stream < 0) {
    best_stream = 0;
  }
  selected_stream_index_ = tracks_[best_stream].stream_index;
  total_frame_count_ = tracks_[best_stream].frame_count;

  // Open codec for selected stream
  AVStream* stream = fmt_ctx_->streams[selected_stream_index_];
  const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
  if (!codec) {
    OutputError(0, AVERROR_DECODER_NOT_FOUND,
                "No decoder found for codec: " +
                    std::string(avcodec_get_name(stream->codecpar->codec_id)));
    return false;
  }

  codec_ctx_ = raii::MakeAvCodecContext(codec);
  if (!codec_ctx_) {
    OutputError(0, AVERROR(ENOMEM), "Failed to allocate codec context");
    return false;
  }

  ret = avcodec_parameters_to_context(codec_ctx_.get(), stream->codecpar);
  if (ret < 0) {
    OutputError(0, ret, "Failed to copy codec params: " + errors::FfmpegErrorString(ret));
    return false;
  }

  // Set threading
  codec_ctx_->thread_count = 0;  // Auto
  codec_ctx_->thread_type = FF_THREAD_FRAME;

  ret = avcodec_open2(codec_ctx_.get(), codec, nullptr);
  if (ret < 0) {
    OutputError(0, ret, "Failed to open codec: " + errors::FfmpegErrorString(ret));
    return false;
  }

  current_frame_index_ = 0;
  configured_ = true;

  // Signal track info to ImageDecoder
  OutputTrackInfo(tracks_, best_stream);

  // For BufferSource (non-streaming), signal completed immediately
  // For streaming mode, OutputCompleted is called from OnStreamEnd
  if (!is_streaming_) {
    OutputCompleted();
  }

  return true;
}

void ImageDecoderWorker::OnStreamData(const ImageStreamDataMessage& msg) {
  // [SPEC] Fetch Stream Data Loop - chunk steps:
  // 1. If [[closed]] is true, abort
  // 2. If chunk is not Uint8Array, close with DataError (handled in JS)
  // 3. Append bytes to [[encoded data]]
  // 4. If [[tracks established]] is false, run Establish Tracks
  // 5. Otherwise, run Update Tracks

  // Append chunk to accumulated data
  image_data_.insert(image_data_.end(), msg.chunk.begin(), msg.chunk.end());

  // Try to configure if not yet done
  if (!configured_) {
    // Try to configure with accumulated data
    // This may fail if not enough data yet - that's OK, we'll try again
    // when more data arrives
    bool success = TryConfigureFromBuffer();
    if (!success && !image_data_.empty()) {
      // Failed to configure - clear the I/O context we may have partially created
      // The error will be reported when stream ends or more data doesn't help
      io_ctx_.reset();
      fmt_ctx_.reset();
      codec_ctx_.reset();
    }
  }
  // Note: For streaming mode, we don't support updating tracks after initial
  // configuration (progressive decoding) - this is a simplification.
}

void ImageDecoderWorker::OnStreamEnd() {
  // [SPEC] Fetch Stream Data Loop - close steps:
  // 1. Assign true to [[complete]]
  // 2. Resolve [[completed promise]]

  stream_complete_ = true;

  if (!configured_) {
    // Try one last time to configure with all accumulated data
    bool success = TryConfigureFromBuffer();
    if (!success) {
      // Not enough data to decode - signal error
      OutputError(0, AVERROR_INVALIDDATA,
                  "Stream ended with insufficient data to decode image");
      return;
    }
  }

  // Signal that stream is complete
  OutputCompleted();
}

void ImageDecoderWorker::OnStreamError(const ImageStreamErrorMessage& msg) {
  // [SPEC] Fetch Stream Data Loop - error steps:
  // Close ImageDecoder with NotReadableError
  OutputError(0, AVERROR(EIO), "Stream error: " + msg.message);
}

void ImageDecoderWorker::OnDecode(const ImageDecodeMessage& msg) {
  if (!codec_ctx_ || !fmt_ctx_) {
    OutputError(msg.promise_id, AVERROR_INVALIDDATA, "Decoder not configured");
    return;
  }

  // Validate frame index
  if (msg.frame_index >= total_frame_count_) {
    OutputError(msg.promise_id, AVERROR(EINVAL),
                "Frame index " + std::to_string(msg.frame_index) +
                    " out of range (max: " + std::to_string(total_frame_count_ - 1) + ")");
    return;
  }

  // Seek to frame if necessary
  if (msg.frame_index != current_frame_index_) {
    if (!SeekToFrame(msg.frame_index)) {
      OutputError(msg.promise_id, AVERROR(EIO),
                  "Failed to seek to frame " + std::to_string(msg.frame_index));
      return;
    }
  }

  // Decode frame
  int64_t timestamp = 0;
  int64_t duration = 0;
  raii::AVFramePtr frame = DecodeNextFrame(&timestamp, &duration);

  if (!frame) {
    OutputError(msg.promise_id, AVERROR(EIO), "Failed to decode frame");
    return;
  }

  current_frame_index_ = msg.frame_index + 1;

  // Build result
  ImageDecodeResult result;
  result.frame = std::move(frame);
  result.timestamp = timestamp;
  result.duration = duration;
  result.complete = true;

  OutputDecodeResult(msg.promise_id, std::move(result));
}

void ImageDecoderWorker::OnReset() {
  // Flush codec buffers
  if (codec_ctx_) {
    avcodec_flush_buffers(codec_ctx_.get());
  }

  // Reset to beginning
  current_frame_index_ = 0;

  // Seek I/O context back to start
  if (io_ctx_) {
    auto* io_ctx_data = static_cast<IOContext*>(io_ctx_->opaque);
    if (io_ctx_data) {
      io_ctx_data->position = 0;
    }
  }

  // Seek format context
  if (fmt_ctx_ && selected_stream_index_ >= 0) {
    av_seek_frame(fmt_ctx_.get(), selected_stream_index_, 0, AVSEEK_FLAG_BACKWARD);
  }
}

void ImageDecoderWorker::OnClose() {
  // Release FFmpeg resources in reverse order of creation
  codec_ctx_.reset();
  fmt_ctx_.reset();
  io_ctx_.reset();
  image_data_.clear();
  tracks_.clear();
}

void ImageDecoderWorker::OnUpdateTrack(const ImageUpdateTrackMessage& msg) {
  if (msg.selected_index < 0 ||
      static_cast<size_t>(msg.selected_index) >= tracks_.size()) {
    return;  // Invalid index, ignore
  }

  int new_stream_index = tracks_[msg.selected_index].stream_index;
  if (new_stream_index == selected_stream_index_) {
    return;  // Same track, nothing to do
  }

  // Close current codec
  codec_ctx_.reset();

  // Open new codec for selected stream
  selected_stream_index_ = new_stream_index;
  total_frame_count_ = tracks_[msg.selected_index].frame_count;

  AVStream* stream = fmt_ctx_->streams[selected_stream_index_];
  const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
  if (!codec) {
    OutputError(0, AVERROR_DECODER_NOT_FOUND, "No decoder for new track");
    return;
  }

  codec_ctx_ = raii::MakeAvCodecContext(codec);
  if (!codec_ctx_) {
    OutputError(0, AVERROR(ENOMEM), "Failed to allocate codec context");
    return;
  }

  int ret = avcodec_parameters_to_context(codec_ctx_.get(), stream->codecpar);
  if (ret < 0) {
    OutputError(0, ret, "Failed to copy codec params");
    return;
  }

  codec_ctx_->thread_count = 0;
  codec_ctx_->thread_type = FF_THREAD_FRAME;

  ret = avcodec_open2(codec_ctx_.get(), codec, nullptr);
  if (ret < 0) {
    OutputError(0, ret, "Failed to open codec");
    return;
  }

  current_frame_index_ = 0;
}

// =============================================================================
// OUTPUT HELPERS
// =============================================================================

void ImageDecoderWorker::OutputTrackInfo(
    const std::vector<ImageTrackInfo>& tracks, int32_t selected_index) {
  if (track_info_callback_) {
    track_info_callback_(tracks, selected_index);
  }
}

void ImageDecoderWorker::OutputDecodeResult(uint32_t promise_id,
                                             ImageDecodeResult result) {
  if (decode_result_callback_) {
    decode_result_callback_(promise_id, std::move(result));
  }
}

void ImageDecoderWorker::OutputError(uint32_t promise_id, int error_code,
                                      const std::string& message) {
  if (error_callback_) {
    error_callback_(promise_id, error_code, message);
  }
}

void ImageDecoderWorker::OutputCompleted() {
  if (completed_callback_) {
    completed_callback_();
  }
}

// =============================================================================
// DECODE HELPERS
// =============================================================================

bool ImageDecoderWorker::SeekToFrame(uint32_t frame_index) {
  if (!fmt_ctx_ || !codec_ctx_) {
    return false;
  }

  // For frame 0, just reset
  if (frame_index == 0) {
    OnReset();
    return true;
  }

  // For animated images, we need to decode frames sequentially
  // because most image formats don't support random access
  if (frame_index < current_frame_index_) {
    // Need to go backwards - reset and decode forward
    OnReset();
  }

  // Decode frames until we reach the target
  while (current_frame_index_ < frame_index) {
    int64_t ts, dur;
    raii::AVFramePtr frame = DecodeNextFrame(&ts, &dur);
    if (!frame) {
      return false;
    }
    current_frame_index_++;
  }

  return true;
}

raii::AVFramePtr ImageDecoderWorker::DecodeNextFrame(int64_t* timestamp,
                                                      int64_t* duration) {
  if (!fmt_ctx_ || !codec_ctx_) {
    return nullptr;
  }

  raii::AVPacketPtr packet = raii::MakeAvPacket();
  raii::AVFramePtr frame = raii::MakeAvFrame();

  if (!packet || !frame) {
    return nullptr;
  }

  AVStream* stream = fmt_ctx_->streams[selected_stream_index_];

  // Read packets and decode
  while (!ShouldExit()) {
    int ret = av_read_frame(fmt_ctx_.get(), packet.get());

    if (ret == AVERROR_EOF) {
      // Try to flush decoder
      ret = avcodec_send_packet(codec_ctx_.get(), nullptr);
      if (ret < 0 && ret != AVERROR_EOF) {
        return nullptr;
      }

      ret = avcodec_receive_frame(codec_ctx_.get(), frame.get());
      if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
        return nullptr;  // No more frames
      }
      if (ret < 0) {
        return nullptr;
      }

      // Got a frame from flush
      if (timestamp) {
        *timestamp = frame->pts != AV_NOPTS_VALUE
                         ? av_rescale_q(frame->pts, stream->time_base,
                                        AVRational{1, 1000000})
                         : 0;
      }
      if (duration) {
        *duration = frame->duration > 0
                        ? av_rescale_q(frame->duration, stream->time_base,
                                       AVRational{1, 1000000})
                        : 0;
      }
      return frame;
    }

    if (ret < 0) {
      return nullptr;  // Read error
    }

    // Skip packets from other streams
    if (packet->stream_index != selected_stream_index_) {
      av_packet_unref(packet.get());
      continue;
    }

    // Send packet to decoder
    ret = avcodec_send_packet(codec_ctx_.get(), packet.get());
    av_packet_unref(packet.get());

    if (ret < 0 && ret != AVERROR(EAGAIN)) {
      return nullptr;
    }

    // Try to receive frame
    ret = avcodec_receive_frame(codec_ctx_.get(), frame.get());

    if (ret == AVERROR(EAGAIN)) {
      continue;  // Need more input
    }
    if (ret < 0) {
      return nullptr;  // Decode error
    }

    // Got a frame
    if (timestamp) {
      *timestamp = frame->pts != AV_NOPTS_VALUE
                       ? av_rescale_q(frame->pts, stream->time_base,
                                      AVRational{1, 1000000})
                       : 0;
    }
    if (duration) {
      *duration = frame->duration > 0
                      ? av_rescale_q(frame->duration, stream->time_base,
                                     AVRational{1, 1000000})
                      : 0;
    }
    return frame;
  }

  return nullptr;  // Exiting
}

}  // namespace webcodecs
