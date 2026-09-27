#pragma once

#include <ArduinoJson.h>

#include "app_event.h"

class AppEventQueue;
class ConfigurationStore;
class Transport;

class CommandDispatcher {
 public:
  explicit CommandDispatcher(ConfigurationStore &configuration)
      : configuration_(configuration) {}

  /** Processes at most one queued event and may enqueue its response. */
  void processOne(AppEventQueue &event_queue, Transport &transport);

 private:
  void processTransportRequest(const AppEvent &event,
                               AppEventQueue &event_queue);
  void processGet(JsonObjectConst request, AppEventQueue &event_queue);
  void processSet(JsonObjectConst request, AppEventQueue &event_queue);
  bool enqueueResponse(const JsonDocument &response,
                       AppEventQueue &event_queue);
  bool enqueueError(const char *error, AppEventQueue &event_queue);

  ConfigurationStore &configuration_;
};
