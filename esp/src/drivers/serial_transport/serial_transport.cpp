#include "serial_transport.h"

#include <Arduino.h>

#include "../../application/app_event_queue.h"

namespace {

constexpr uint32_t kBaudRate = 115200;
constexpr size_t kMaxBytesPerProcess = 64;
constexpr char kQueueFullResponse[] =
    "{\"ok\":false,\"error\":\"queue_full\"}";

}  // namespace

void SerialTransport::begin() {
  Serial.begin(kBaudRate);
}

void SerialTransport::process(AppEventQueue &event_queue) {
  size_t processed = 0;
  while (processed < kMaxBytesPerProcess && Serial.available() > 0) {
    const uint8_t byte = static_cast<uint8_t>(Serial.read());
    ++processed;
    if (codec_.consume(byte, event_queue) ==
        FrameCodec::ConsumeResult::QueueFull) {
      sendQueueFull();
    }
  }
}

bool SerialTransport::sendResponse(const uint8_t *payload,
                                   size_t payload_size) {
  return sendFrame(FrameCodec::kResponseType, payload, payload_size);
}

bool SerialTransport::sendData(const uint8_t *payload, size_t payload_size) {
  return sendFrame(2, payload, payload_size);
}

bool SerialTransport::sendFrame(uint8_t type, const uint8_t *payload,
                                size_t payload_size) {
  uint8_t header[FrameCodec::kHeaderSize];
  uint8_t crc[FrameCodec::kCrcSize];
  if (!codec_.encode(type, payload, payload_size, header, crc)) {
    return false;
  }
  return Serial.write(header, sizeof(header)) == sizeof(header) &&
         Serial.write(payload, payload_size) == payload_size &&
         Serial.write(crc, sizeof(crc)) == sizeof(crc);
}

void SerialTransport::sendQueueFull() {
  sendResponse(reinterpret_cast<const uint8_t *>(kQueueFullResponse),
               sizeof(kQueueFullResponse) - 1);
}
