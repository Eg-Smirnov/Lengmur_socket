#include "acquisition_task.h"

#include <string.h>

bool AcquisitionTask::start(
    const AcquisitionConfig &initial_configuration) {
  if (task_ != nullptr) {
    return false;
  }

  command_queue_ = xQueueCreate(kQueueCapacity, sizeof(AcquisitionCommand));
  result_queue_ = xQueueCreate(kQueueCapacity, sizeof(AcquisitionResult));
  if (command_queue_ == nullptr || result_queue_ == nullptr) {
    if (command_queue_ != nullptr) {
      vQueueDelete(command_queue_);
      command_queue_ = nullptr;
    }
    if (result_queue_ != nullptr) {
      vQueueDelete(result_queue_);
      result_queue_ = nullptr;
    }
    return false;
  }

  initial_configuration_ = initial_configuration;
  if (xTaskCreatePinnedToCore(taskEntry, "AcquisitionTask", kStackSize, this,
                              kPriority, &task_, kCore) == pdPASS) {
    return true;
  }

  vQueueDelete(command_queue_);
  vQueueDelete(result_queue_);
  command_queue_ = nullptr;
  result_queue_ = nullptr;
  task_ = nullptr;
  return false;
}

bool AcquisitionTask::send(const AcquisitionCommand &command) {
  return command_queue_ != nullptr &&
         xQueueSend(command_queue_, &command, 0) == pdTRUE;
}

bool AcquisitionTask::receive(AcquisitionResult &result,
                              TickType_t ticks_to_wait) {
  return result_queue_ != nullptr &&
         xQueueReceive(result_queue_, &result, ticks_to_wait) == pdTRUE;
}

bool AcquisitionTask::copySweep(ad7322::SamplePair *samples,
                                size_t capacity, size_t &sample_count) {
  if (state_ != State::SweepReady || samples == nullptr ||
      capacity < sweep_size_) {
    return false;
  }
  memcpy(samples, sweep_samples_, sweep_size_ * sizeof(sweep_samples_[0]));
  sample_count = sweep_size_;
  return true;
}

void AcquisitionTask::taskEntry(void *context) {
  static_cast<AcquisitionTask *>(context)->run();
}

void AcquisitionTask::run() {
  adc_pair_.begin();
  if (!acquisitionBegin(initial_configuration_)) {
    state_ = State::Fault;
    publish(AcquisitionResultType::InitializationFailed, 0);
  } else {
    active_configuration_ = initial_configuration_;
    state_ = State::Idle;
    last_telemetry_us_ = micros();
    publish(AcquisitionResultType::Ready, 0);
  }

  AcquisitionCommand command{};
  while (true) {
    if (xQueueReceive(command_queue_, &command, pdMS_TO_TICKS(1)) == pdTRUE) {
      handle(command);
    }
    tick();
  }
}

void AcquisitionTask::handle(const AcquisitionCommand &command) {
  switch (command.type) {
    case AcquisitionCommandType::PrepareConfig:
      handlePrepare(command);
      break;
    case AcquisitionCommandType::CommitConfig:
      handleCommit(command);
      break;
    case AcquisitionCommandType::RollbackConfig:
      handleRollback(command);
      break;
    case AcquisitionCommandType::StartSweep:
      if (state_ != State::Idle || active_configuration_.sample_rate <= 0 ||
          active_configuration_.min_voltage >
              active_configuration_.max_voltage) {
        publish(AcquisitionResultType::SweepRejected, 0);
      } else {
        sweep_size_ = 0;
        last_sample_us_ = 0;
        state_ = State::WaitingForSweepWindow;
        publish(AcquisitionResultType::SweepStarted, 0);
      }
      break;
    case AcquisitionCommandType::ReleaseSweep:
      if (state_ == State::SweepReady) {
        state_ = State::Idle;
      }
      break;
  }
}

void AcquisitionTask::handlePrepare(const AcquisitionCommand &command) {
  if (state_ != State::Idle) {
    publish(AcquisitionResultType::ConfigRejected, command.transaction_id);
    return;
  }

  previous_configuration_ = active_configuration_;
  if (!acquisitionApplyConfig(command.configuration)) {
    if (!acquisitionApplyConfig(previous_configuration_)) {
      state_ = State::Fault;
    }
    publish(AcquisitionResultType::ConfigRejected, command.transaction_id);
    return;
  }

  prepared_configuration_ = command.configuration;
  pending_transaction_id_ = command.transaction_id;
  state_ = State::AwaitingDecision;
  publish(AcquisitionResultType::ConfigPrepared, command.transaction_id);
}

void AcquisitionTask::tick() {
  if (state_ == State::Fault || state_ == State::Starting ||
      state_ == State::AwaitingDecision || state_ == State::SweepReady) {
    return;
  }

  const uint32_t now = micros();
  const bool capturing = state_ == State::CapturingSweep;
  const uint32_t interval_us = capturing
                                   ? (1000000U / static_cast<uint32_t>(
                                                     active_configuration_.sample_rate))
                                   : 500000U;
  const uint32_t period_us = interval_us == 0 ? 1 : interval_us;
  const uint32_t previous_us = capturing ? last_sample_us_ : last_telemetry_us_;
  if (now - previous_us < period_us) {
    return;
  }

  ad7322::SamplePair sample{};
  if (!adc_pair_.read(sample)) {
    state_ = State::Fault;
    sweep_size_ = 0;
    publish(AcquisitionResultType::SweepFailed, 0);
    return;
  }

  if (!capturing) {
    last_telemetry_us_ = now;
    if (state_ == State::Idle) {
      publishSample(AcquisitionResultType::Telemetry, sample);
    }
    if (state_ == State::WaitingForSweepWindow &&
        sample.voltage >= active_configuration_.min_voltage &&
        sample.voltage <= active_configuration_.max_voltage) {
      state_ = State::CapturingSweep;
      last_sample_us_ = now - period_us;
    }
    return;
  }

  if (sample.voltage < active_configuration_.min_voltage ||
      sample.voltage > active_configuration_.max_voltage ||
      sweep_size_ >= kMaxSweepPoints) {
    state_ = State::SweepReady;
    publish(AcquisitionResultType::SweepCompleted, 0);
    return;
  }

  sweep_samples_[sweep_size_++] = sample;
  last_sample_us_ = now;
  if (sweep_size_ >= kMaxSweepPoints) {
    state_ = State::SweepReady;
    publish(AcquisitionResultType::SweepCompleted, 0);
  }
}

void AcquisitionTask::handleCommit(const AcquisitionCommand &command) {
  if (state_ != State::AwaitingDecision ||
      command.transaction_id != pending_transaction_id_) {
    publish(AcquisitionResultType::ConfigRejected, command.transaction_id);
    return;
  }

  active_configuration_ = prepared_configuration_;
  pending_transaction_id_ = 0;
  state_ = State::Idle;
  publish(AcquisitionResultType::ConfigCommitted, command.transaction_id);
}

void AcquisitionTask::handleRollback(const AcquisitionCommand &command) {
  if (state_ != State::AwaitingDecision ||
      command.transaction_id != pending_transaction_id_) {
    publish(AcquisitionResultType::ConfigRollbackFailed,
            command.transaction_id);
    return;
  }

  if (!acquisitionApplyConfig(previous_configuration_)) {
    state_ = State::Fault;
    publish(AcquisitionResultType::ConfigRollbackFailed,
            command.transaction_id);
    return;
  }

  pending_transaction_id_ = 0;
  state_ = State::Idle;
  publish(AcquisitionResultType::ConfigRolledBack,
          command.transaction_id);
}

void AcquisitionTask::publish(AcquisitionResultType type,
                              uint32_t transaction_id) {
  const AcquisitionResult result{type, transaction_id, 0, 0};
  xQueueSend(result_queue_, &result, portMAX_DELAY);
}

void AcquisitionTask::publishSample(AcquisitionResultType type,
                                    const ad7322::SamplePair &sample) {
  const AcquisitionResult result{type, 0, sample.voltage, sample.current};
  xQueueSend(result_queue_, &result, portMAX_DELAY);
}
