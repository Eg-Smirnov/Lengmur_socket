#pragma once

#include <stdint.h>

#include "acquisition.h"

enum class AcquisitionCommandType : uint8_t {
  PrepareConfig,
  CommitConfig,
  RollbackConfig,
};

struct AcquisitionCommand {
  AcquisitionCommandType type;
  uint32_t transaction_id;
  AcquisitionConfig configuration;
};

enum class AcquisitionResultType : uint8_t {
  Ready,
  InitializationFailed,
  ConfigPrepared,
  ConfigRejected,
  ConfigCommitted,
  ConfigRolledBack,
  ConfigRollbackFailed,
};

struct AcquisitionResult {
  AcquisitionResultType type;
  uint32_t transaction_id;
};
