#include "command_dispatcher.h"

#include <string.h>

#include "../acquisition/acquisition_task.h"
#include "../transport/transport.h"
#include "app_event_queue.h"
#include "configuration/configuration_store.h"

void CommandDispatcher::processOne(AppEventQueue &event_queue,
                                   Transport &transport) {
  AppEvent event{};
  if (!event_queue.pop(event)) {
    return;
  }

  switch (event.command) {
    case AppCommand::ProcessTransportRequest:
      processTransportRequest(event, event_queue);
      break;
    case AppCommand::SendTransportResponse:
      transport.sendResponse(event.payload, event.payload_size);
      break;
    case AppCommand::InvalidCrc:
      enqueueError("invalid_crc", event_queue);
      break;
    case AppCommand::UnsupportedVersion:
      enqueueError("unsupported_version", event_queue);
      break;
    case AppCommand::UnexpectedPacketType:
      enqueueError("unexpected_packet_type", event_queue);
      break;
    case AppCommand::RequestTooLarge:
      enqueueError("request_too_large", event_queue);
      break;
    default:
      break;
  }
}

void CommandDispatcher::processTransportRequest(
    const AppEvent &event, AppEventQueue &event_queue) {
  JsonDocument request_document;
  if (deserializeJson(request_document, event.payload, event.payload_size) !=
          DeserializationError::Ok ||
      !request_document.is<JsonObject>()) {
    enqueueError("invalid_json", event_queue);
    return;
  }

  JsonObjectConst request = request_document.as<JsonObjectConst>();
  const char *operation = request["op"];
  if (operation == nullptr) {
    enqueueError("invalid_operation", event_queue);
  } else if (strcmp(operation, "get") == 0) {
    processGet(request, event_queue);
  } else if (strcmp(operation, "set") == 0) {
    processSet(request, event_queue);
  } else {
    enqueueError("invalid_operation", event_queue);
  }
}

void CommandDispatcher::processGet(JsonObjectConst request,
                                   AppEventQueue &event_queue) {
  JsonDocument response;
  response["ok"] = true;
  if (request.containsKey("field")) {
    const char *field = request["field"];
    if (!configuration_.hasField(field)) {
      enqueueError("unknown_field", event_queue);
      return;
    }
    response["field"] = field;
    response["value"].set(configuration_.current()[field]);
  } else {
    response["config"].set(configuration_.current());
  }
  enqueueResponse(response, event_queue);
}

void CommandDispatcher::processSet(JsonObjectConst request,
                                   AppEventQueue &event_queue) {
  if (transaction_.active()) {
    enqueueError("queue_full", event_queue);
    return;
  }

  JsonDocument candidate;
  const char *validation_error = nullptr;
  if (!configuration_.buildCandidate(request, candidate, validation_error)) {
    enqueueError(validation_error, event_queue);
    return;
  }

  AcquisitionConfig new_configuration{};
  if (!configuration_.getAcquisitionConfig(candidate, new_configuration)) {
    enqueueError("invalid_config", event_queue);
    return;
  }

  uint32_t transaction_id = next_transaction_id_++;
  if (transaction_id == 0) {
    transaction_id = next_transaction_id_++;
  }
  if (!pending_candidate_.set(candidate) || pending_candidate_.overflowed() ||
      !transaction_.begin(transaction_id)) {
    clearPendingCandidate();
    enqueueError("invalid_config", event_queue);
    return;
  }

  const AcquisitionCommand command{AcquisitionCommandType::PrepareConfig,
                                   transaction_id, new_configuration};
  if (!acquisition_.send(command)) {
    transaction_.reject(transaction_id);
    clearPendingCandidate();
    enqueueError("queue_full", event_queue);
  }
}

void CommandDispatcher::processPendingAcquisition() {
  AcquisitionCommandType type;
  switch (transaction_.state()) {
    case ConfigurationTransaction::State::NeedCommit:
      type = AcquisitionCommandType::CommitConfig;
      break;
    case ConfigurationTransaction::State::NeedRollback:
      type = AcquisitionCommandType::RollbackConfig;
      break;
    default:
      return;
  }

  const AcquisitionCommand command{type, transaction_.transactionId(), {}};
  if (acquisition_.send(command)) {
    transaction_.decisionSent();
  }
}

void CommandDispatcher::processAcquisitionResult(
    const AcquisitionResult &result, AppEventQueue &event_queue) {
  switch (result.type) {
    case AcquisitionResultType::ConfigPrepared: {
      if (transaction_.state() !=
              ConfigurationTransaction::State::AwaitingPrepare ||
          result.transaction_id != transaction_.transactionId()) {
        return;
      }
      const bool saved = configuration_.save(pending_candidate_);
      transaction_.prepared(result.transaction_id, saved);
      break;
    }
    case AcquisitionResultType::ConfigRejected:
      if (transaction_.reject(result.transaction_id)) {
        clearPendingCandidate();
        enqueueError("configuration_apply_error", event_queue);
      }
      break;
    case AcquisitionResultType::ConfigCommitted:
      if (transaction_.finish(
              result.transaction_id,
              ConfigurationTransaction::State::AwaitingCommit)) {
        configuration_.publish(pending_candidate_);
        clearPendingCandidate();
        enqueueSuccess(event_queue);
      }
      break;
    case AcquisitionResultType::ConfigRolledBack:
    case AcquisitionResultType::ConfigRollbackFailed:
      if (transaction_.finish(
              result.transaction_id,
              ConfigurationTransaction::State::AwaitingRollback)) {
        clearPendingCandidate();
        enqueueError("storage_error", event_queue);
      }
      break;
    default:
      break;
  }
}

bool CommandDispatcher::enqueueResponse(const JsonDocument &response,
                                        AppEventQueue &event_queue) {
  const size_t payload_size = measureJson(response);
  if (payload_size > kAppEventPayloadSize) {
    return false;
  }

  uint8_t payload[kAppEventPayloadSize];
  if (serializeJson(response, payload, sizeof(payload)) != payload_size) {
    return false;
  }
  return event_queue.push(AppCommand::SendTransportResponse, payload,
                          payload_size);
}

bool CommandDispatcher::enqueueError(const char *error,
                                     AppEventQueue &event_queue) {
  JsonDocument response;
  response["ok"] = false;
  response["error"] = error;
  return enqueueResponse(response, event_queue);
}

bool CommandDispatcher::enqueueSuccess(AppEventQueue &event_queue) {
  JsonDocument response;
  response["ok"] = true;
  return enqueueResponse(response, event_queue);
}

void CommandDispatcher::clearPendingCandidate() {
  pending_candidate_.clear();
}
