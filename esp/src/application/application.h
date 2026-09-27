#pragma once

#include "../drivers/serial_transport/serial_transport.h"
#include "../transport/frame_codec.h"
#include "app_event_queue.h"
#include "command_dispatcher.h"
#include "configuration/configuration_store.h"

class Application {
 public:
  Application();

  /** Initializes configuration, application hardware, and the active transport. */
  void begin();

  /** Runs one nonblocking transport and dispatcher iteration. */
  void process();

 private:
  ConfigurationStore configuration_;
  AppEventQueue event_queue_;
  FrameCodec codec_;
  SerialTransport transport_;
  CommandDispatcher dispatcher_;
};
