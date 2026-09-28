#include "application.h"

Application::Application()
    : transport_(codec_), dispatcher_(configuration_, acquisition_) {}

void Application::begin() {
  if (control_task_ != nullptr) {
    return;
  }
  xTaskCreatePinnedToCore(controlTaskEntry, "ControlTask", kControlStackSize,
                          this, kControlPriority, &control_task_,
                          kControlCore);
}

void Application::controlTaskEntry(void *context) {
  static_cast<Application *>(context)->runControlTask();
}

void Application::runControlTask() {
  configuration_.begin();

  AcquisitionConfig acquisition_configuration{};
  if (configuration_.getAcquisitionConfig(configuration_.current(),
                                          acquisition_configuration)) {
    acquisition_.start(acquisition_configuration);
    AcquisitionResult startup_result{};
    acquisition_.receive(startup_result, pdMS_TO_TICKS(1000));
  }
  transport_.begin();

  while (true) {
    AcquisitionResult result{};
    while (acquisition_.receive(result)) {
      dispatcher_.processAcquisitionResult(result, event_queue_);
    }
    dispatcher_.processPendingAcquisition();
    transport_.process(event_queue_);
    dispatcher_.processOne(event_queue_, transport_);
    vTaskDelay(1);
  }
}
