#include "frame_codec.h"

#include <limits.h>
#include <string.h>

#include "../application/app_event_queue.h"

namespace {

constexpr uint8_t kMagic0 = 0x4c;  // L
constexpr uint8_t kMagic1 = 0x53;  // S
constexpr uint8_t kProtocolVersion = 1;
constexpr uint8_t kCommandType = 0;

uint16_t readU16(const uint8_t *source) {
  return static_cast<uint16_t>(source[0]) |
         (static_cast<uint16_t>(source[1]) << 8);
}

uint32_t readU32(const uint8_t *source) {
  return static_cast<uint32_t>(source[0]) |
         (static_cast<uint32_t>(source[1]) << 8) |
         (static_cast<uint32_t>(source[2]) << 16) |
         (static_cast<uint32_t>(source[3]) << 24);
}

void writeU32(uint8_t *target, uint32_t value) {
  target[0] = static_cast<uint8_t>(value);
  target[1] = static_cast<uint8_t>(value >> 8);
  target[2] = static_cast<uint8_t>(value >> 16);
  target[3] = static_cast<uint8_t>(value >> 24);
}

uint32_t updateCrc32(uint32_t crc, const uint8_t *data, size_t length) {
  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
  }
  return crc;
}

uint32_t crc32(const uint8_t *data, size_t length) {
  return ~updateCrc32(0xffffffffu, data, length);
}

}  // namespace

FrameCodec::ConsumeResult FrameCodec::consume(uint8_t byte,
                                               AppEventQueue &event_queue) {
  if (receive_length_ == sizeof(receive_buffer_)) {
    discardFirstByte();
  }
  receive_buffer_[receive_length_++] = byte;
  return processReceiveBuffer(event_queue);
}

bool FrameCodec::encode(uint8_t type, const uint8_t *payload,
                        size_t payload_size,
                        uint8_t (&header)[kHeaderSize],
                        uint8_t (&crc)[kCrcSize]) const {
  if (type > 7 || payload_size > kMaxFramePayloadSize ||
      payload_size > UINT16_MAX || (payload_size > 0 && payload == nullptr)) {
    return false;
  }

  header[0] = kMagic0;
  header[1] = kMagic1;
  header[2] = static_cast<uint8_t>((type << 5) | kProtocolVersion);
  header[3] = static_cast<uint8_t>(payload_size);
  header[4] = static_cast<uint8_t>(payload_size >> 8);

  uint32_t value = updateCrc32(0xffffffffu, header, sizeof(header));
  value = ~updateCrc32(value, payload, payload_size);
  writeU32(crc, value);
  return true;
}

FrameCodec::ConsumeResult FrameCodec::processReceiveBuffer(
    AppEventQueue &event_queue) {
  while (receive_length_ >= 2) {
    if (receive_buffer_[0] != kMagic0 || receive_buffer_[1] != kMagic1) {
      discardFirstByte();
      continue;
    }
    if (receive_length_ < kHeaderSize) {
      return ConsumeResult::Accepted;
    }

    const uint8_t type_and_version = receive_buffer_[2];
    const uint8_t type = type_and_version >> 5;
    const uint8_t version = type_and_version & 0x1f;
    const size_t payload_size = readU16(receive_buffer_ + 3);
    if (payload_size > kMaxFramePayloadSize) {
      discardFirstByte();
      continue;
    }

    const size_t frame_size = kHeaderSize + payload_size + kCrcSize;
    if (receive_length_ < frame_size) {
      return ConsumeResult::Accepted;
    }

    AppCommand command = AppCommand::ProcessTransportRequest;
    const uint8_t *event_payload = receive_buffer_ + kHeaderSize;
    size_t event_payload_size = payload_size;
    const uint32_t expected_crc =
        readU32(receive_buffer_ + kHeaderSize + payload_size);
    if (crc32(receive_buffer_, kHeaderSize + payload_size) != expected_crc) {
      command = AppCommand::InvalidCrc;
      event_payload = nullptr;
      event_payload_size = 0;
    } else if (version != kProtocolVersion) {
      command = AppCommand::UnsupportedVersion;
      event_payload = nullptr;
      event_payload_size = 0;
    } else if (type != kCommandType) {
      command = AppCommand::UnexpectedPacketType;
      event_payload = nullptr;
      event_payload_size = 0;
    } else if (payload_size > kAppEventPayloadSize) {
      command = AppCommand::RequestTooLarge;
      event_payload = nullptr;
      event_payload_size = 0;
    }

    const bool queued =
        event_queue.push(command, event_payload, event_payload_size);
    discardFrame(frame_size);
    if (!queued) {
      return ConsumeResult::QueueFull;
    }
  }
  return ConsumeResult::Accepted;
}

void FrameCodec::discardFirstByte() {
  if (receive_length_ > 0) {
    memmove(receive_buffer_, receive_buffer_ + 1, --receive_length_);
  }
}

void FrameCodec::discardFrame(size_t frame_size) {
  receive_length_ -= frame_size;
  memmove(receive_buffer_, receive_buffer_ + frame_size, receive_length_);
}
