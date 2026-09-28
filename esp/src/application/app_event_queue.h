#pragma once

#include <stddef.h>
#include <stdint.h>

#include "app_event.h"

class AppEventQueue {
 public:
  static constexpr size_t kCapacity = 8;

  /** Copies one event while reserving the last slot for a transport response. */
  bool push(AppCommand command, const uint8_t *payload = nullptr,
            size_t payload_size = 0);

  /** Removes the oldest event and copies it to `event`. */
  bool pop(AppEvent &event);

  /** Reports whether the FIFO contains no events. */
  bool empty() const;

 private:
  AppEvent events_[kCapacity]{};
  size_t head_ = 0;
  size_t size_ = 0;
};
