#include "acquisition_task.h"

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

void AcquisitionTask::taskEntry(void *context) {
  static_cast<AcquisitionTask *>(context)->run();
}

void AcquisitionTask::run() {
  if (!acquisitionBegin(initial_configuration_)) {
    state_ = State::Fault;
    publish(AcquisitionResultType::InitializationFailed, 0);
  } else {
    active_configuration_ = initial_configuration_;
    state_ = State::Idle;
    publish(AcquisitionResultType::Ready, 0);
  }

  AcquisitionCommand command{};
  while (true) {
    if (xQueueReceive(command_queue_, &command, portMAX_DELAY) == pdTRUE) {
      handle(command);
    }
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
  const AcquisitionResult result{type, transaction_id};
  xQueueSend(result_queue_, &result, portMAX_DELAY);
}
