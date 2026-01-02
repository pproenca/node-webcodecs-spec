#pragma once
/**
 * packet_pool.h - Global Packet Pool with Observability
 *
 * Designed for Meta/Facebook scale encoder output paths.
 *
 * Packets are simpler than frames - they're variable-sized buffers
 * without dimension requirements. But pooling still helps:
 * - Reduces allocation overhead for encoder output
 * - Prevents fragmentation from frequent alloc/free
 * - Provides observability for production debugging
 *
 * Thread Safety:
 * - All operations are mutex-protected
 * - Statistics can be read lock-free via atomics
 */

#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <cstdint>

extern "C" {
#include <libavcodec/packet.h>
}

namespace webcodecs {

// ===========================================================================
// POOL STATISTICS
// ===========================================================================

/**
 * Statistics for packet pool observability.
 */
struct PacketPoolStats {
  std::atomic<uint64_t> total_allocated{0};
  std::atomic<uint64_t> pool_hits{0};
  std::atomic<uint64_t> pool_misses{0};
  std::atomic<uint64_t> current_in_flight{0};
  std::atomic<uint64_t> current_pooled{0};
  std::atomic<uint64_t> peak_in_flight{0};
  std::atomic<uint64_t> total_bytes_allocated{0};  // Track memory usage

  void reset() {
    total_allocated.store(0, std::memory_order_relaxed);
    pool_hits.store(0, std::memory_order_relaxed);
    pool_misses.store(0, std::memory_order_relaxed);
    current_in_flight.store(0, std::memory_order_relaxed);
    current_pooled.store(0, std::memory_order_relaxed);
    peak_in_flight.store(0, std::memory_order_relaxed);
    total_bytes_allocated.store(0, std::memory_order_relaxed);
  }

  [[nodiscard]] double hit_rate() const {
    uint64_t hits = pool_hits.load(std::memory_order_relaxed);
    uint64_t misses = pool_misses.load(std::memory_order_relaxed);
    uint64_t total = hits + misses;
    return total > 0 ? static_cast<double>(hits) / total : 0.0;
  }
};

// ===========================================================================
// GLOBAL PACKET POOL
// ===========================================================================

/**
 * Global packet pool for encoder output.
 *
 * Unlike frames, packets don't have fixed dimensions, so we use a single pool.
 * Packets are unreferenced before returning to pool, so buffer data is freed.
 */
class GlobalPacketPool {
 public:
  /**
   * RAII deleter that returns packet to pool.
   */
  struct PooledPacketDeleter {
    GlobalPacketPool* pool;

    void operator()(AVPacket* packet) const noexcept {
      if (pool && packet) {
        pool->ReturnPacket(packet);
      } else if (packet) {
        av_packet_free(&packet);
      }
    }
  };

  using PooledPacket = std::unique_ptr<AVPacket, PooledPacketDeleter>;

  // ---------------------------------------------------------------------------
  // SINGLETON
  // ---------------------------------------------------------------------------

  static GlobalPacketPool& instance() {
    static GlobalPacketPool pool;
    return pool;
  }

  // ---------------------------------------------------------------------------
  // CONFIGURATION
  // ---------------------------------------------------------------------------

  void SetMaxPoolSize(size_t max_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_pool_size_ = max_size;
  }

  // ---------------------------------------------------------------------------
  // ACQUIRE / RETURN
  // ---------------------------------------------------------------------------

  /**
   * Acquire a packet from the pool.
   * Packet is automatically returned when PooledPacket is destroyed.
   *
   * @return PooledPacket smart pointer, or nullptr on allocation failure
   */
  [[nodiscard]] PooledPacket acquire() {
    std::lock_guard<std::mutex> lock(mutex_);

    AVPacket* packet = nullptr;

    if (!pool_.empty()) {
      packet = pool_.back();
      pool_.pop_back();
      stats_.pool_hits.fetch_add(1, std::memory_order_relaxed);
      stats_.current_pooled.fetch_sub(1, std::memory_order_relaxed);
    } else {
      packet = av_packet_alloc();
      if (!packet) {
        return nullptr;
      }
      stats_.pool_misses.fetch_add(1, std::memory_order_relaxed);
      stats_.total_allocated.fetch_add(1, std::memory_order_relaxed);
    }

    uint64_t in_flight = stats_.current_in_flight.fetch_add(1, std::memory_order_relaxed) + 1;

    uint64_t peak = stats_.peak_in_flight.load(std::memory_order_relaxed);
    while (in_flight > peak) {
      if (stats_.peak_in_flight.compare_exchange_weak(peak, in_flight, std::memory_order_relaxed)) {
        break;
      }
    }

    return PooledPacket(packet, PooledPacketDeleter{this});
  }

  /**
   * Acquire a packet and copy data from source.
   * Creates a reference to the source packet's data.
   *
   * @param src Source packet to reference
   * @return PooledPacket with referenced data, or nullptr on failure
   */
  [[nodiscard]] PooledPacket acquire_ref(const AVPacket* src) {
    if (!src) return nullptr;

    auto packet = acquire();
    if (!packet) return nullptr;

    if (av_packet_ref(packet.get(), src) < 0) {
      return nullptr;
    }

    stats_.total_bytes_allocated.fetch_add(src->size, std::memory_order_relaxed);
    return packet;
  }

  /**
   * Acquire a packet and allocate buffer.
   *
   * @param size Buffer size to allocate
   * @return PooledPacket with allocated buffer, or nullptr on failure
   */
  [[nodiscard]] PooledPacket acquire_with_buffer(int size) {
    auto packet = acquire();
    if (!packet) return nullptr;

    if (av_new_packet(packet.get(), size) < 0) {
      return nullptr;
    }

    stats_.total_bytes_allocated.fetch_add(size, std::memory_order_relaxed);
    return packet;
  }

  // ---------------------------------------------------------------------------
  // STATISTICS
  // ---------------------------------------------------------------------------

  [[nodiscard]] const PacketPoolStats& stats() const { return stats_; }

  void ResetStats() { stats_.reset(); }

  [[nodiscard]] size_t pooled_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_.size();
  }

  // ---------------------------------------------------------------------------
  // CLEANUP
  // ---------------------------------------------------------------------------

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (AVPacket* packet : pool_) {
      av_packet_free(&packet);
    }
    pool_.clear();
    stats_.current_pooled.store(0, std::memory_order_relaxed);
  }

  void trim(size_t target_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    while (pool_.size() > target_size) {
      AVPacket* packet = pool_.back();
      pool_.pop_back();
      av_packet_free(&packet);
      stats_.current_pooled.fetch_sub(1, std::memory_order_relaxed);
    }
  }

 private:
  GlobalPacketPool() = default;

  ~GlobalPacketPool() { clear(); }

  GlobalPacketPool(const GlobalPacketPool&) = delete;
  GlobalPacketPool& operator=(const GlobalPacketPool&) = delete;

  void ReturnPacket(AVPacket* packet) {
    if (!packet) return;

    av_packet_unref(packet);

    std::lock_guard<std::mutex> lock(mutex_);

    stats_.current_in_flight.fetch_sub(1, std::memory_order_relaxed);

    if (pool_.size() < max_pool_size_) {
      pool_.push_back(packet);
      stats_.current_pooled.fetch_add(1, std::memory_order_relaxed);
    } else {
      av_packet_free(&packet);
    }
  }

  mutable std::mutex mutex_;
  std::vector<AVPacket*> pool_;
  PacketPoolStats stats_;
  size_t max_pool_size_{64};
};

// ===========================================================================
// POOL HANDLE
// ===========================================================================

/**
 * RAII handle for packet pool access.
 */
class PacketPoolHandle {
 public:
  PacketPoolHandle() : pool_(&GlobalPacketPool::instance()) {}

  [[nodiscard]] GlobalPacketPool::PooledPacket acquire() { return pool_->acquire(); }

  [[nodiscard]] GlobalPacketPool::PooledPacket acquire_ref(const AVPacket* src) { return pool_->acquire_ref(src); }

  [[nodiscard]] GlobalPacketPool::PooledPacket acquire_with_buffer(int size) {
    return pool_->acquire_with_buffer(size);
  }

  [[nodiscard]] const PacketPoolStats& stats() const { return pool_->stats(); }

 private:
  GlobalPacketPool* pool_;
};

}  // namespace webcodecs
