#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "acquisition_messages.h"
#include "../drivers/ad7322/ad7322_pair.h"

class AcquisitionTask {
 public:
  static constexpr BaseType_t kCore = 0;
  static constexpr UBaseType_t kPriority = 3;
  static constexpr uint32_t kStackSize = 4096;
  static constexpr UBaseType_t kQueueCapacity = 4;
  static constexpr size_t kMaxSweepPoints = 100;

  /** Creates the inter-core queues and starts the acquisition owner task. */
  bool start(const AcquisitionConfig &initial_configuration);

  /** Enqueues a control command without waiting for queue space. */
  bool send(const AcquisitionCommand &command);

  /** Receives one acquisition result, optionally waiting for `ticks_to_wait`. */
  bool receive(AcquisitionResult &result, TickType_t ticks_to_wait = 0);
  bool copySweep(ad7322::SamplePair *samples, size_t capacity,
                 size_t &sample_count);

 private:
  enum class State : uint8_t {
    Starting,
    Idle,
    AwaitingDecision,
    WaitingForSweepWindow,
    CapturingSweep,
    SweepReady,
    Fault
  };

  static void taskEntry(void *context);
  void run();
  void handle(const AcquisitionCommand &command);
  void handlePrepare(const AcquisitionCommand &command);
  void handleCommit(const AcquisitionCommand &command);
  void handleRollback(const AcquisitionCommand &command);
  void tick();
  void publish(AcquisitionResultType type, uint32_t transaction_id);
  void publishSample(AcquisitionResultType type,
                     const ad7322::SamplePair &sample);

  QueueHandle_t command_queue_ = nullptr;
  QueueHandle_t result_queue_ = nullptr;
  TaskHandle_t task_ = nullptr;
  AcquisitionConfig initial_configuration_{};
  AcquisitionConfig active_configuration_{};
  AcquisitionConfig previous_configuration_{};
  AcquisitionConfig prepared_configuration_{};
  uint32_t pending_transaction_id_ = 0;
  ad7322::Pair adc_pair_;
  ad7322::SamplePair sweep_samples_[kMaxSweepPoints]{};
  size_t sweep_size_ = 0;
  uint32_t last_sample_us_ = 0;
  uint32_t last_telemetry_us_ = 0;
  State state_ = State::Starting;
};
