#pragma once

#include <stddef.h>
#include <stdint.h>

class AppEventQueue;

class FrameCodec {
 public:
  static constexpr size_t kHeaderSize = 5;
  static constexpr size_t kCrcSize = 4;
  static constexpr size_t kMaxFramePayloadSize = 2048;
  static constexpr uint8_t kResponseType = 1;

  enum class ConsumeResult : uint8_t {
    Accepted,
    QueueFull,
  };

  /** Consumes one stream byte and emits any completed frame into the queue. */
  ConsumeResult consume(uint8_t byte, AppEventQueue &event_queue);

  /** Builds the frame header and CRC for a payload without copying the payload. */
  bool encode(uint8_t type, const uint8_t *payload, size_t payload_size,
              uint8_t (&header)[kHeaderSize], uint8_t (&crc)[kCrcSize]) const;

 private:
  static constexpr size_t kReceiveBufferSize =
      kHeaderSize + kMaxFramePayloadSize + kCrcSize;

  ConsumeResult processReceiveBuffer(AppEventQueue &event_queue);
  void discardFirstByte();
  void discardFrame(size_t frame_size);

  uint8_t receive_buffer_[kReceiveBufferSize]{};
  size_t receive_length_ = 0;
};
