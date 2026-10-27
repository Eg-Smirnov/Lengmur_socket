#pragma once

#include <ArduinoJson.h>

#include "../acquisition/acquisition_messages.h"
#include "app_event.h"
#include "configuration_transaction.h"

class AppEventQueue;
class AcquisitionTask;
class ConfigurationStore;
class Transport;

class CommandDispatcher {
 public:
  CommandDispatcher(ConfigurationStore &configuration,
                    AcquisitionTask &acquisition)
      : configuration_(configuration), acquisition_(acquisition) {}

  /** Processes at most one queued event and may enqueue its response. */
  void processOne(AppEventQueue &event_queue, Transport &transport);

  /** Advances a pending configuration decision when its queue has space. */
  void processPendingAcquisition();

  /** Handles one result returned by the acquisition owner task. */
  void processAcquisitionResult(const AcquisitionResult &result,
                                AppEventQueue &event_queue);
  void sendSweepData(Transport &transport);

 private:
  void processTransportRequest(const AppEvent &event,
                               AppEventQueue &event_queue);
  void processGet(JsonObjectConst request, AppEventQueue &event_queue);
  void processSet(JsonObjectConst request, AppEventQueue &event_queue);
  bool enqueueResponse(const JsonDocument &response,
                       AppEventQueue &event_queue);
  bool enqueueError(const char *error, AppEventQueue &event_queue);
  bool enqueueSuccess(AppEventQueue &event_queue);
  void clearPendingCandidate();

  ConfigurationStore &configuration_;
  AcquisitionTask &acquisition_;
  ConfigurationTransaction transaction_;
  JsonDocument pending_candidate_;
  uint32_t next_transaction_id_ = 1;
  bool sweep_pending_ = false;
};
