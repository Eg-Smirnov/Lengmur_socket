#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr size_t kAppEventPayloadSize = 256;

enum class AppCommand : uint8_t {
  ProcessTransportRequest,
  SendTransportResponse,
  InvalidCrc,
  UnsupportedVersion,
  UnexpectedPacketType,
  RequestTooLarge,
  ProcessAcquiredData,
  ProcessStoredSweep,
  SendSweepData,
  SendStoredSweep,
  StartMeasurement,
  StopMeasurement,
};

struct AppEvent {
  AppCommand command;
  uint16_t payload_size;
  alignas(4) uint8_t payload[kAppEventPayloadSize];
};

static_assert(sizeof(AppEvent) == 260, "Unexpected AppEvent layout");
