/**
 * WebCodecs API Type Definitions for Node.js
 * Auto-generated from W3C WebIDL specification
 * 
 * @packageDocumentation
 * @see https://www.w3.org/TR/webcodecs/
 */

// --- Required DOM polyfill types for Node.js ---
// These types are needed because Node.js doesn't have DOM globals

export type BufferSource = ArrayBufferView | ArrayBuffer;
export type AllowSharedBufferSource = ArrayBufferView | ArrayBuffer | SharedArrayBuffer;
export type EventHandler = ((event: Event) => void) | null;

// BitrateMode is defined in MediaStream Recording spec, used by AudioEncoderConfig
export type BitrateMode = 'constant' | 'variable';

export interface DOMRectInit {
  height?: number;
  width?: number;
  x?: number;
  y?: number;
}

export interface DOMRectReadOnly {
  readonly bottom: number;
  readonly height: number;
  readonly left: number;
  readonly right: number;
  readonly top: number;
  readonly width: number;
  readonly x: number;
  readonly y: number;
}

// Placeholder types for browser APIs not available in Node.js
export type CanvasImageSource = unknown;
export type ImageBitmap = unknown;
export type OffscreenCanvas = unknown;
export type PredefinedColorSpace = 'display-p3' | 'srgb';
export type ColorSpaceConversion = 'default' | 'none';

// --- WebCodecs Types (from W3C spec) ---

export interface AudioDecoder extends EventTarget {
  // constructor(init: AudioDecoderInit)
  readonly state: CodecState;
  readonly decodeQueueSize: number;
  ondequeue: EventHandler;
  configure(config: AudioDecoderConfig): void;
  decode(chunk: EncodedAudioChunk): void;
  flush(): Promise<void>;
  reset(): void;
  close(): void;
  // static isConfigSupported(config: AudioDecoderConfig): Promise<AudioDecoderSupport>;
}

export interface AudioDecoderInit {
  output: AudioDataOutputCallback;
  error: WebCodecsErrorCallback;
}

export type AudioDataOutputCallback = (output: AudioData) => void;

export interface VideoDecoder extends EventTarget {
  // constructor(init: VideoDecoderInit)
  readonly state: CodecState;
  readonly decodeQueueSize: number;
  ondequeue: EventHandler;
  configure(config: VideoDecoderConfig): void;
  decode(chunk: EncodedVideoChunk): void;
  flush(): Promise<void>;
  reset(): void;
  close(): void;
  // static isConfigSupported(config: VideoDecoderConfig): Promise<VideoDecoderSupport>;
}

export interface VideoDecoderInit {
  output: VideoFrameOutputCallback;
  error: WebCodecsErrorCallback;
}

export type VideoFrameOutputCallback = (output: VideoFrame) => void;

export interface AudioEncoder extends EventTarget {
  // constructor(init: AudioEncoderInit)
  readonly state: CodecState;
  readonly encodeQueueSize: number;
  ondequeue: EventHandler;
  configure(config: AudioEncoderConfig): void;
  encode(data: AudioData): void;
  flush(): Promise<void>;
  reset(): void;
  close(): void;
  // static isConfigSupported(config: AudioEncoderConfig): Promise<AudioEncoderSupport>;
}

export interface AudioEncoderInit {
  output: EncodedAudioChunkOutputCallback;
  error: WebCodecsErrorCallback;
}

export type EncodedAudioChunkOutputCallback = (output: EncodedAudioChunk, metadata?: EncodedAudioChunkMetadata) => void;

export interface EncodedAudioChunkMetadata {
  decoderConfig?: AudioDecoderConfig;
}

export interface VideoEncoder extends EventTarget {
  // constructor(init: VideoEncoderInit)
  readonly state: CodecState;
  readonly encodeQueueSize: number;
  ondequeue: EventHandler;
  configure(config: VideoEncoderConfig): void;
  encode(frame: VideoFrame, options?: VideoEncoderEncodeOptions): void;
  flush(): Promise<void>;
  reset(): void;
  close(): void;
  // static isConfigSupported(config: VideoEncoderConfig): Promise<VideoEncoderSupport>;
}

export interface VideoEncoderInit {
  output: EncodedVideoChunkOutputCallback;
  error: WebCodecsErrorCallback;
}

export type EncodedVideoChunkOutputCallback = (chunk: EncodedVideoChunk, metadata?: EncodedVideoChunkMetadata) => void;

export interface EncodedVideoChunkMetadata {
  decoderConfig?: VideoDecoderConfig;
  svc?: SvcOutputMetadata;
  alphaSideData?: BufferSource;
}

export interface SvcOutputMetadata {
  temporalLayerId?: number;
}

export interface AudioDecoderSupport {
  supported?: boolean;
  config?: AudioDecoderConfig;
}

export interface VideoDecoderSupport {
  supported?: boolean;
  config?: VideoDecoderConfig;
}

export interface AudioEncoderSupport {
  supported?: boolean;
  config?: AudioEncoderConfig;
}

export interface VideoEncoderSupport {
  supported?: boolean;
  config?: VideoEncoderConfig;
}

export interface AudioDecoderConfig {
  codec: string;
  sampleRate: number;
  numberOfChannels: number;
  description?: AllowSharedBufferSource;
}

export interface VideoDecoderConfig {
  codec: string;
  description?: AllowSharedBufferSource;
  codedWidth?: number;
  codedHeight?: number;
  displayAspectWidth?: number;
  displayAspectHeight?: number;
  colorSpace?: VideoColorSpaceInit;
  hardwareAcceleration?: HardwareAcceleration;
  optimizeForLatency?: boolean;
  rotation?: number;
  flip?: boolean;
}

export interface AudioEncoderConfig {
  codec: string;
  sampleRate: number;
  numberOfChannels: number;
  bitrate?: number;
  bitrateMode?: BitrateMode;
}

export interface VideoEncoderConfig {
  codec: string;
  width: number;
  height: number;
  displayWidth?: number;
  displayHeight?: number;
  bitrate?: number;
  framerate?: number;
  hardwareAcceleration?: HardwareAcceleration;
  alpha?: AlphaOption;
  scalabilityMode?: string;
  bitrateMode?: VideoEncoderBitrateMode;
  latencyMode?: LatencyMode;
  contentHint?: string;
}

export type HardwareAcceleration = 'no-preference' | 'prefer-hardware' | 'prefer-software';

export type AlphaOption = 'keep' | 'discard';

export type LatencyMode = 'quality' | 'realtime';

export interface VideoEncoderEncodeOptions {
  keyFrame?: boolean;
}

export type VideoEncoderBitrateMode = 'constant' | 'variable' | 'quantizer';

export type CodecState = 'unconfigured' | 'configured' | 'closed';

export type WebCodecsErrorCallback = (error: DOMException) => void;

export interface EncodedAudioChunk {
  // constructor(init: EncodedAudioChunkInit)
  readonly type: EncodedAudioChunkType;
  readonly timestamp: number;
  readonly duration: number | null;
  readonly byteLength: number;
  copyTo(destination: AllowSharedBufferSource): void;
}

export interface EncodedAudioChunkInit {
  type: EncodedAudioChunkType;
  timestamp: number;
  duration?: number;
  data: AllowSharedBufferSource;
  transfer?: ArrayBuffer[];
}

export type EncodedAudioChunkType = 'key' | 'delta';

export interface EncodedVideoChunk {
  // constructor(init: EncodedVideoChunkInit)
  readonly type: EncodedVideoChunkType;
  readonly timestamp: number;
  readonly duration: number | null;
  readonly byteLength: number;
  copyTo(destination: AllowSharedBufferSource): void;
}

export interface EncodedVideoChunkInit {
  type: EncodedVideoChunkType;
  timestamp: number;
  duration?: number;
  data: AllowSharedBufferSource;
  transfer?: ArrayBuffer[];
}

export type EncodedVideoChunkType = 'key' | 'delta';

export interface AudioData {
  // constructor(init: AudioDataInit)
  readonly format: AudioSampleFormat | null;
  readonly sampleRate: number;
  readonly numberOfFrames: number;
  readonly numberOfChannels: number;
  readonly duration: number;
  readonly timestamp: number;
  allocationSize(options: AudioDataCopyToOptions): number;
  copyTo(destination: AllowSharedBufferSource, options: AudioDataCopyToOptions): void;
  clone(): AudioData;
  close(): void;
}

export interface AudioDataInit {
  format: AudioSampleFormat;
  sampleRate: number;
  numberOfFrames: number;
  numberOfChannels: number;
  timestamp: number;
  data: BufferSource;
  transfer?: ArrayBuffer[];
}

export interface AudioDataCopyToOptions {
  planeIndex: number;
  frameOffset?: number;
  frameCount?: number;
  format?: AudioSampleFormat;
}

export type AudioSampleFormat = 'u8' | 's16' | 's32' | 'f32' | 'u8-planar' | 's16-planar' | 's32-planar' | 'f32-planar';

export interface VideoFrame {
  // constructor(image: CanvasImageSource, init?: VideoFrameInit)
  // constructor(data: AllowSharedBufferSource, init: VideoFrameBufferInit)
  readonly format: VideoPixelFormat | null;
  readonly codedWidth: number;
  readonly codedHeight: number;
  readonly codedRect: DOMRectReadOnly | null;
  readonly visibleRect: DOMRectReadOnly | null;
  readonly rotation: number;
  readonly flip: boolean;
  readonly displayWidth: number;
  readonly displayHeight: number;
  readonly duration: number | null;
  readonly timestamp: number;
  readonly colorSpace: VideoColorSpace;
  metadata(): VideoFrameMetadata;
  allocationSize(options?: VideoFrameCopyToOptions): number;
  copyTo(destination: AllowSharedBufferSource, options?: VideoFrameCopyToOptions): Promise<PlaneLayout[]>;
  clone(): VideoFrame;
  close(): void;
}

export interface VideoFrameInit {
  duration?: number;
  timestamp?: number;
  alpha?: AlphaOption;
  visibleRect?: DOMRectInit;
  rotation?: number;
  flip?: boolean;
  displayWidth?: number;
  displayHeight?: number;
  metadata?: VideoFrameMetadata;
}

export interface VideoFrameBufferInit {
  format: VideoPixelFormat;
  codedWidth: number;
  codedHeight: number;
  timestamp: number;
  duration?: number;
  layout?: PlaneLayout[];
  visibleRect?: DOMRectInit;
  rotation?: number;
  flip?: boolean;
  displayWidth?: number;
  displayHeight?: number;
  colorSpace?: VideoColorSpaceInit;
  transfer?: ArrayBuffer[];
  metadata?: VideoFrameMetadata;
}

/** VideoFrameMetadata is extensible per WebCodecs spec */
export type VideoFrameMetadata = Record<string, unknown>;

export interface VideoFrameCopyToOptions {
  rect?: DOMRectInit;
  layout?: PlaneLayout[];
  format?: VideoPixelFormat;
  colorSpace?: PredefinedColorSpace;
}

export interface PlaneLayout {
  offset: number;
  stride: number;
}

export type VideoPixelFormat = 'I420' | 'I420P10' | 'I420P12' | 'I420A' | 'I420AP10' | 'I420AP12' | 'I422' | 'I422P10' | 'I422P12' | 'I422A' | 'I422AP10' | 'I422AP12' | 'I444' | 'I444P10' | 'I444P12' | 'I444A' | 'I444AP10' | 'I444AP12' | 'NV12' | 'RGBA' | 'RGBX' | 'BGRA' | 'BGRX';

export interface VideoColorSpace {
  // constructor(init?: VideoColorSpaceInit)
  readonly primaries: VideoColorPrimaries | null;
  readonly transfer: VideoTransferCharacteristics | null;
  readonly matrix: VideoMatrixCoefficients | null;
  readonly fullRange: boolean | null;
  toJSON(): VideoColorSpaceInit;
}

export interface VideoColorSpaceInit {
  primaries?: VideoColorPrimaries | null;
  transfer?: VideoTransferCharacteristics | null;
  matrix?: VideoMatrixCoefficients | null;
  fullRange?: boolean | null;
}

export type VideoColorPrimaries = 'bt709' | 'bt470bg' | 'smpte170m' | 'bt2020' | 'smpte432';

export type VideoTransferCharacteristics = 'bt709' | 'smpte170m' | 'iec61966-2-1' | 'linear' | 'pq' | 'hlg';

export type VideoMatrixCoefficients = 'rgb' | 'bt709' | 'bt470bg' | 'smpte170m' | 'bt2020-ncl';

export interface ImageDecoder {
  // constructor(init: ImageDecoderInit)
  readonly type: string;
  readonly complete: boolean;
  readonly completed: Promise<void>;
  readonly tracks: ImageTrackList;
  decode(options?: ImageDecodeOptions): Promise<ImageDecodeResult>;
  reset(): void;
  close(): void;
  // static isTypeSupported(type: string): Promise<boolean>;
}

export type ImageBufferSource = AllowSharedBufferSource | ReadableStream;

export interface ImageDecoderInit {
  type: string;
  data: ImageBufferSource;
  colorSpaceConversion?: ColorSpaceConversion;
  desiredWidth?: number;
  desiredHeight?: number;
  preferAnimation?: boolean;
  transfer?: ArrayBuffer[];
}

export interface ImageDecodeOptions {
  frameIndex?: number;
  completeFramesOnly?: boolean;
}

export interface ImageDecodeResult {
  image: VideoFrame;
  complete: boolean;
}

export interface ImageTrackList {
  readonly ready: Promise<void>;
  readonly length: number;
  readonly selectedIndex: number;
  readonly selectedTrack: ImageTrack | null;
}

export interface ImageTrack {
  readonly animated: boolean;
  readonly frameCount: number;
  readonly repetitionCount: number;
  selected: boolean;
}

export interface AudioDecoder extends EventTarget {
  // constructor(init: AudioDecoderInit)
  readonly state: CodecState;
  readonly decodeQueueSize: number;
  ondequeue: EventHandler;
  configure(config: AudioDecoderConfig): void;
  decode(chunk: EncodedAudioChunk): void;
  flush(): Promise<void>;
  reset(): void;
  close(): void;
  // static isConfigSupported(config: AudioDecoderConfig): Promise<AudioDecoderSupport>;
}

export interface AudioDecoderInit {
  output: AudioDataOutputCallback;
  error: WebCodecsErrorCallback;
}

export type AudioDataOutputCallback = (output: AudioData) => void;

export interface VideoDecoder extends EventTarget {
  // constructor(init: VideoDecoderInit)
  readonly state: CodecState;
  readonly decodeQueueSize: number;
  ondequeue: EventHandler;
  configure(config: VideoDecoderConfig): void;
  decode(chunk: EncodedVideoChunk): void;
  flush(): Promise<void>;
  reset(): void;
  close(): void;
  // static isConfigSupported(config: VideoDecoderConfig): Promise<VideoDecoderSupport>;
}

export interface VideoDecoderInit {
  output: VideoFrameOutputCallback;
  error: WebCodecsErrorCallback;
}

export type VideoFrameOutputCallback = (output: VideoFrame) => void;

export interface AudioEncoder extends EventTarget {
  // constructor(init: AudioEncoderInit)
  readonly state: CodecState;
  readonly encodeQueueSize: number;
  ondequeue: EventHandler;
  configure(config: AudioEncoderConfig): void;
  encode(data: AudioData): void;
  flush(): Promise<void>;
  reset(): void;
  close(): void;
  // static isConfigSupported(config: AudioEncoderConfig): Promise<AudioEncoderSupport>;
}

export interface AudioEncoderInit {
  output: EncodedAudioChunkOutputCallback;
  error: WebCodecsErrorCallback;
}

export type EncodedAudioChunkOutputCallback = (output: EncodedAudioChunk, metadata?: EncodedAudioChunkMetadata) => void;

export interface EncodedAudioChunkMetadata {
  decoderConfig?: AudioDecoderConfig;
}

export interface VideoEncoder extends EventTarget {
  // constructor(init: VideoEncoderInit)
  readonly state: CodecState;
  readonly encodeQueueSize: number;
  ondequeue: EventHandler;
  configure(config: VideoEncoderConfig): void;
  encode(frame: VideoFrame, options?: VideoEncoderEncodeOptions): void;
  flush(): Promise<void>;
  reset(): void;
  close(): void;
  // static isConfigSupported(config: VideoEncoderConfig): Promise<VideoEncoderSupport>;
}

export interface VideoEncoderInit {
  output: EncodedVideoChunkOutputCallback;
  error: WebCodecsErrorCallback;
}

export type EncodedVideoChunkOutputCallback = (chunk: EncodedVideoChunk, metadata?: EncodedVideoChunkMetadata) => void;

export interface EncodedVideoChunkMetadata {
  decoderConfig?: VideoDecoderConfig;
  svc?: SvcOutputMetadata;
  alphaSideData?: BufferSource;
}

export interface SvcOutputMetadata {
  temporalLayerId?: number;
}

export interface AudioDecoderSupport {
  supported?: boolean;
  config?: AudioDecoderConfig;
}

export interface VideoDecoderSupport {
  supported?: boolean;
  config?: VideoDecoderConfig;
}

export interface AudioEncoderSupport {
  supported?: boolean;
  config?: AudioEncoderConfig;
}

export interface VideoEncoderSupport {
  supported?: boolean;
  config?: VideoEncoderConfig;
}

export interface AudioDecoderConfig {
  codec: string;
  sampleRate: number;
  numberOfChannels: number;
  description?: AllowSharedBufferSource;
}

export interface VideoDecoderConfig {
  codec: string;
  description?: AllowSharedBufferSource;
  codedWidth?: number;
  codedHeight?: number;
  displayAspectWidth?: number;
  displayAspectHeight?: number;
  colorSpace?: VideoColorSpaceInit;
  hardwareAcceleration?: HardwareAcceleration;
  optimizeForLatency?: boolean;
  rotation?: number;
  flip?: boolean;
}

export interface AudioEncoderConfig {
  codec: string;
  sampleRate: number;
  numberOfChannels: number;
  bitrate?: number;
  bitrateMode?: BitrateMode;
}

export interface VideoEncoderConfig {
  codec: string;
  width: number;
  height: number;
  displayWidth?: number;
  displayHeight?: number;
  bitrate?: number;
  framerate?: number;
  hardwareAcceleration?: HardwareAcceleration;
  alpha?: AlphaOption;
  scalabilityMode?: string;
  bitrateMode?: VideoEncoderBitrateMode;
  latencyMode?: LatencyMode;
  contentHint?: string;
}

export type HardwareAcceleration = 'no-preference' | 'prefer-hardware' | 'prefer-software';

export type AlphaOption = 'keep' | 'discard';

export type LatencyMode = 'quality' | 'realtime';

export interface VideoEncoderEncodeOptions {
  keyFrame?: boolean;
}

export type VideoEncoderBitrateMode = 'constant' | 'variable' | 'quantizer';

export type CodecState = 'unconfigured' | 'configured' | 'closed';

export type WebCodecsErrorCallback = (error: DOMException) => void;

export interface EncodedAudioChunk {
  // constructor(init: EncodedAudioChunkInit)
  readonly type: EncodedAudioChunkType;
  readonly timestamp: number;
  readonly duration: number | null;
  readonly byteLength: number;
  copyTo(destination: AllowSharedBufferSource): void;
}

export interface EncodedAudioChunkInit {
  type: EncodedAudioChunkType;
  timestamp: number;
  duration?: number;
  data: AllowSharedBufferSource;
  transfer?: ArrayBuffer[];
}

export type EncodedAudioChunkType = 'key' | 'delta';

export interface EncodedVideoChunk {
  // constructor(init: EncodedVideoChunkInit)
  readonly type: EncodedVideoChunkType;
  readonly timestamp: number;
  readonly duration: number | null;
  readonly byteLength: number;
  copyTo(destination: AllowSharedBufferSource): void;
}

export interface EncodedVideoChunkInit {
  type: EncodedVideoChunkType;
  timestamp: number;
  duration?: number;
  data: AllowSharedBufferSource;
  transfer?: ArrayBuffer[];
}

export type EncodedVideoChunkType = 'key' | 'delta';

export interface AudioData {
  // constructor(init: AudioDataInit)
  readonly format: AudioSampleFormat | null;
  readonly sampleRate: number;
  readonly numberOfFrames: number;
  readonly numberOfChannels: number;
  readonly duration: number;
  readonly timestamp: number;
  allocationSize(options: AudioDataCopyToOptions): number;
  copyTo(destination: AllowSharedBufferSource, options: AudioDataCopyToOptions): void;
  clone(): AudioData;
  close(): void;
}

export interface AudioDataInit {
  format: AudioSampleFormat;
  sampleRate: number;
  numberOfFrames: number;
  numberOfChannels: number;
  timestamp: number;
  data: BufferSource;
  transfer?: ArrayBuffer[];
}

export interface AudioDataCopyToOptions {
  planeIndex: number;
  frameOffset?: number;
  frameCount?: number;
  format?: AudioSampleFormat;
}

export type AudioSampleFormat = 'u8' | 's16' | 's32' | 'f32' | 'u8-planar' | 's16-planar' | 's32-planar' | 'f32-planar';

export interface VideoFrame {
  // constructor(image: CanvasImageSource, init?: VideoFrameInit)
  // constructor(data: AllowSharedBufferSource, init: VideoFrameBufferInit)
  readonly format: VideoPixelFormat | null;
  readonly codedWidth: number;
  readonly codedHeight: number;
  readonly codedRect: DOMRectReadOnly | null;
  readonly visibleRect: DOMRectReadOnly | null;
  readonly rotation: number;
  readonly flip: boolean;
  readonly displayWidth: number;
  readonly displayHeight: number;
  readonly duration: number | null;
  readonly timestamp: number;
  readonly colorSpace: VideoColorSpace;
  metadata(): VideoFrameMetadata;
  allocationSize(options?: VideoFrameCopyToOptions): number;
  copyTo(destination: AllowSharedBufferSource, options?: VideoFrameCopyToOptions): Promise<PlaneLayout[]>;
  clone(): VideoFrame;
  close(): void;
}

export interface VideoFrameInit {
  duration?: number;
  timestamp?: number;
  alpha?: AlphaOption;
  visibleRect?: DOMRectInit;
  rotation?: number;
  flip?: boolean;
  displayWidth?: number;
  displayHeight?: number;
  metadata?: VideoFrameMetadata;
}

export interface VideoFrameBufferInit {
  format: VideoPixelFormat;
  codedWidth: number;
  codedHeight: number;
  timestamp: number;
  duration?: number;
  layout?: PlaneLayout[];
  visibleRect?: DOMRectInit;
  rotation?: number;
  flip?: boolean;
  displayWidth?: number;
  displayHeight?: number;
  colorSpace?: VideoColorSpaceInit;
  transfer?: ArrayBuffer[];
  metadata?: VideoFrameMetadata;
}

/** VideoFrameMetadata is extensible per WebCodecs spec */
export type VideoFrameMetadata = Record<string, unknown>;

export interface VideoFrameCopyToOptions {
  rect?: DOMRectInit;
  layout?: PlaneLayout[];
  format?: VideoPixelFormat;
  colorSpace?: PredefinedColorSpace;
}

export interface PlaneLayout {
  offset: number;
  stride: number;
}

export type VideoPixelFormat = 'I420' | 'I420P10' | 'I420P12' | 'I420A' | 'I420AP10' | 'I420AP12' | 'I422' | 'I422P10' | 'I422P12' | 'I422A' | 'I422AP10' | 'I422AP12' | 'I444' | 'I444P10' | 'I444P12' | 'I444A' | 'I444AP10' | 'I444AP12' | 'NV12' | 'RGBA' | 'RGBX' | 'BGRA' | 'BGRX';

export interface VideoColorSpace {
  // constructor(init?: VideoColorSpaceInit)
  readonly primaries: VideoColorPrimaries | null;
  readonly transfer: VideoTransferCharacteristics | null;
  readonly matrix: VideoMatrixCoefficients | null;
  readonly fullRange: boolean | null;
  toJSON(): VideoColorSpaceInit;
}

export interface VideoColorSpaceInit {
  primaries?: VideoColorPrimaries | null;
  transfer?: VideoTransferCharacteristics | null;
  matrix?: VideoMatrixCoefficients | null;
  fullRange?: boolean | null;
}

export type VideoColorPrimaries = 'bt709' | 'bt470bg' | 'smpte170m' | 'bt2020' | 'smpte432';

export type VideoTransferCharacteristics = 'bt709' | 'smpte170m' | 'iec61966-2-1' | 'linear' | 'pq' | 'hlg';

export type VideoMatrixCoefficients = 'rgb' | 'bt709' | 'bt470bg' | 'smpte170m' | 'bt2020-ncl';

export interface ImageDecoder {
  // constructor(init: ImageDecoderInit)
  readonly type: string;
  readonly complete: boolean;
  readonly completed: Promise<void>;
  readonly tracks: ImageTrackList;
  decode(options?: ImageDecodeOptions): Promise<ImageDecodeResult>;
  reset(): void;
  close(): void;
  // static isTypeSupported(type: string): Promise<boolean>;
}

export type ImageBufferSource = AllowSharedBufferSource | ReadableStream;

export interface ImageDecoderInit {
  type: string;
  data: ImageBufferSource;
  colorSpaceConversion?: ColorSpaceConversion;
  desiredWidth?: number;
  desiredHeight?: number;
  preferAnimation?: boolean;
  transfer?: ArrayBuffer[];
}

export interface ImageDecodeOptions {
  frameIndex?: number;
  completeFramesOnly?: boolean;
}

export interface ImageDecodeResult {
  image: VideoFrame;
  complete: boolean;
}

export interface ImageTrackList {
  readonly ready: Promise<void>;
  readonly length: number;
  readonly selectedIndex: number;
  readonly selectedTrack: ImageTrack | null;
}

export interface ImageTrack {
  readonly animated: boolean;
  readonly frameCount: number;
  readonly repetitionCount: number;
  selected: boolean;
}
