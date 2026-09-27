#include <assert.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "../../esp/src/application/app_event_queue.h"
#include "../../esp/src/transport/frame_codec.h"

namespace {

uint32_t updateCrc32(uint32_t crc, const uint8_t *data, size_t length) {
  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
  }
  return crc;
}

void writeCrc(std::vector<uint8_t> &frame) {
  const uint32_t crc =
      ~updateCrc32(0xffffffffu, frame.data(), frame.size() - 4);
  const size_t offset = frame.size() - 4;
  frame[offset] = static_cast<uint8_t>(crc);
  frame[offset + 1] = static_cast<uint8_t>(crc >> 8);
  frame[offset + 2] = static_cast<uint8_t>(crc >> 16);
  frame[offset + 3] = static_cast<uint8_t>(crc >> 24);
}

std::vector<uint8_t> makeFrame(FrameCodec &codec, uint8_t type,
                               const std::vector<uint8_t> &payload) {
  uint8_t header[FrameCodec::kHeaderSize];
  uint8_t crc[FrameCodec::kCrcSize];
  assert(codec.encode(type, payload.data(), payload.size(), header, crc));

  std::vector<uint8_t> frame(header, header + sizeof(header));
  frame.insert(frame.end(), payload.begin(), payload.end());
  frame.insert(frame.end(), crc, crc + sizeof(crc));
  return frame;
}

FrameCodec::ConsumeResult feed(FrameCodec &codec, AppEventQueue &queue,
                               const std::vector<uint8_t> &frame) {
  FrameCodec::ConsumeResult result = FrameCodec::ConsumeResult::Accepted;
  for (uint8_t byte : frame) {
    result = codec.consume(byte, queue);
  }
  return result;
}

void expectEmptyEvent(AppEventQueue &queue, AppCommand command) {
  AppEvent event{};
  assert(queue.pop(event));
  assert(event.command == command);
  assert(event.payload_size == 0);
  assert(queue.empty());
}

void testRequest() {
  FrameCodec codec;
  AppEventQueue queue;
  const std::string json = "{\"op\":\"get\"}";
  const std::vector<uint8_t> payload(json.begin(), json.end());
  const std::vector<uint8_t> frame = makeFrame(codec, 0, payload);

  codec.consume(0xff, queue);
  assert(feed(codec, queue, frame) == FrameCodec::ConsumeResult::Accepted);

  AppEvent event{};
  assert(queue.pop(event));
  assert(event.command == AppCommand::ProcessTransportRequest);
  assert(event.payload_size == payload.size());
  assert(std::vector<uint8_t>(event.payload,
                              event.payload + event.payload_size) == payload);
  assert(queue.empty());
}

void testFrameErrors() {
  const std::vector<uint8_t> payload{'{', '}'};

  {
    FrameCodec codec;
    AppEventQueue queue;
    std::vector<uint8_t> frame = makeFrame(codec, 0, payload);
    frame.back() ^= 0x80;
    feed(codec, queue, frame);
    expectEmptyEvent(queue, AppCommand::InvalidCrc);
  }
  {
    FrameCodec codec;
    AppEventQueue queue;
    std::vector<uint8_t> frame = makeFrame(codec, 0, payload);
    frame[2] = static_cast<uint8_t>((frame[2] & 0xe0) | 2);
    writeCrc(frame);
    feed(codec, queue, frame);
    expectEmptyEvent(queue, AppCommand::UnsupportedVersion);
  }
  {
    FrameCodec codec;
    AppEventQueue queue;
    feed(codec, queue, makeFrame(codec, 1, payload));
    expectEmptyEvent(queue, AppCommand::UnexpectedPacketType);
  }
}

void testRequestTooLarge() {
  FrameCodec codec;
  AppEventQueue queue;
  const std::vector<uint8_t> payload(kAppEventPayloadSize + 1, 'x');
  feed(codec, queue, makeFrame(codec, 0, payload));
  expectEmptyEvent(queue, AppCommand::RequestTooLarge);
}

void testQueueFull() {
  FrameCodec codec;
  AppEventQueue queue;
  for (size_t index = 0; index < AppEventQueue::kCapacity; ++index) {
    assert(queue.push(AppCommand::StopMeasurement));
  }
  const std::vector<uint8_t> payload{'{', '}'};
  assert(feed(codec, queue, makeFrame(codec, 0, payload)) ==
         FrameCodec::ConsumeResult::QueueFull);
}

}  // namespace

int main() {
  testRequest();
  testFrameErrors();
  testRequestTooLarge();
  testQueueFull();
  return 0;
}
