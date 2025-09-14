#pragma once

#include <esp_timer.h>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace _since {

// a sensor that remembers when an event occurred
// but publishes how long it has been since that occurance
// and regularly updates that on each poll.
class Since : public PollingComponent, public sensor::Sensor {
 public:
  Since() = default;
  void update() override;
  void set_when(int64_t when = esp_timer_get_time());

 private:
  int64_t when_{-1};
};

}  // namespace _since
}  // namespace esphome