#include "app_event_queue.h"

#include <string.h>

bool AppEventQueue::push(AppCommand command, const uint8_t *payload,
                         size_t payload_size) {
  const size_t capacity = command == AppCommand::SendTransportResponse
                              ? kCapacity
                              : kCapacity - 1;
  if (size_ >= capacity || payload_size > kAppEventPayloadSize ||
      (payload_size > 0 && payload == nullptr)) {
    return false;
  }

  AppEvent &event = events_[(head_ + size_) % kCapacity];
  event.command = command;
  event.payload_size = static_cast<uint16_t>(payload_size);
  if (payload_size > 0) {
    memcpy(event.payload, payload, payload_size);
  }
  ++size_;
  return true;
}

bool AppEventQueue::pop(AppEvent &event) {
  if (size_ == 0) {
    return false;
  }
  event = events_[head_];
  head_ = (head_ + 1) % kCapacity;
  --size_;
  return true;
}

bool AppEventQueue::empty() const {
  return size_ == 0;
}
