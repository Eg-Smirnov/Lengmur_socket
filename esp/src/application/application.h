#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../acquisition/acquisition_task.h"
#include "../drivers/serial_transport/serial_transport.h"
#include "../transport/frame_codec.h"
#include "app_event_queue.h"
#include "command_dispatcher.h"
#include "configuration/configuration_store.h"

class Application {
 public:
  static constexpr BaseType_t kControlCore = 1;
  static constexpr UBaseType_t kControlPriority = 2;
  static constexpr uint32_t kControlStackSize = 8192;

  Application();

  /** Starts the control task pinned to the communication core. */
  void begin();

 private:
  static void controlTaskEntry(void *context);
  void runControlTask();

  ConfigurationStore configuration_;
  AppEventQueue event_queue_;
  FrameCodec codec_;
  SerialTransport transport_;
  AcquisitionTask acquisition_;
  CommandDispatcher dispatcher_;
  TaskHandle_t control_task_ = nullptr;
};
