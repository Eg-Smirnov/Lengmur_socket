#pragma once

#include <stddef.h>
#include <stdint.h>

class AppEventQueue;

class Transport {
 public:
  virtual ~Transport() = default;

  /** Initializes the underlying byte stream. */
  virtual void begin() = 0;

  /** Polls incoming bytes and places completed events into `event_queue`. */
  virtual void process(AppEventQueue &event_queue) = 0;

  /** Encodes and sends one application response payload. */
  virtual bool sendResponse(const uint8_t *payload, size_t payload_size) = 0;
  /** Encodes and sends one binary data payload. */
  virtual bool sendData(const uint8_t *payload, size_t payload_size) = 0;
};
