#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "acquisition_messages.h"

class AcquisitionTask {
 public:
  static constexpr BaseType_t kCore = 0;
  static constexpr UBaseType_t kPriority = 3;
  static constexpr uint32_t kStackSize = 4096;
  static constexpr UBaseType_t kQueueCapacity = 4;

  /** Creates the inter-core queues and starts the acquisition owner task. */
  bool start(const AcquisitionConfig &initial_configuration);

  /** Enqueues a control command without waiting for queue space. */
  bool send(const AcquisitionCommand &command);

  /** Receives one acquisition result, optionally waiting for `ticks_to_wait`. */
  bool receive(AcquisitionResult &result, TickType_t ticks_to_wait = 0);

 private:
  enum class State : uint8_t { Starting, Idle, AwaitingDecision, Fault };

  static void taskEntry(void *context);
  void run();
  void handle(const AcquisitionCommand &command);
  void handlePrepare(const AcquisitionCommand &command);
  void handleCommit(const AcquisitionCommand &command);
  void handleRollback(const AcquisitionCommand &command);
  void publish(AcquisitionResultType type, uint32_t transaction_id);

  QueueHandle_t command_queue_ = nullptr;
  QueueHandle_t result_queue_ = nullptr;
  TaskHandle_t task_ = nullptr;
  AcquisitionConfig initial_configuration_{};
  AcquisitionConfig active_configuration_{};
  AcquisitionConfig previous_configuration_{};
  AcquisitionConfig prepared_configuration_{};
  uint32_t pending_transaction_id_ = 0;
  State state_ = State::Starting;
};
