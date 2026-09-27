#pragma once

#include "../../transport/frame_codec.h"
#include "../../transport/transport.h"

class SerialTransport : public Transport {
 public:
  explicit SerialTransport(FrameCodec &codec) : codec_(codec) {}

  /** Starts USB Serial at the protocol baud rate. */
  void begin() override;

  /** Passes all currently available Serial bytes directly to the frame codec. */
  void process(AppEventQueue &event_queue) override;

  /** Encodes and writes one JSON response frame to Serial. */
  bool sendResponse(const uint8_t *payload, size_t payload_size) override;

 private:
  bool sendFrame(uint8_t type, const uint8_t *payload, size_t payload_size);
  void sendQueueFull();

  FrameCodec &codec_;
};
