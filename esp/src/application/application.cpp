#include "application.h"

#include "../acquisition/acquisition.h"

Application::Application()
    : transport_(codec_), dispatcher_(configuration_) {}

void Application::begin() {
  configuration_.begin();

  AcquisitionConfig acquisition_configuration{};
  if (configuration_.getAcquisitionConfig(configuration_.current(),
                                          acquisition_configuration)) {
    acquisitionBegin(acquisition_configuration);
  }
  transport_.begin();
}

void Application::process() {
  transport_.process(event_queue_);
  dispatcher_.processOne(event_queue_, transport_);
}
