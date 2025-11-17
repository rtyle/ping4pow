#pragma once

#include <esp_timer.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wc++11-compat"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#pragma GCC diagnostic pop


#ifdef USE_LVGL
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#include "esphome/components/lvgl/lvgl_esphome.h"
#pragma GCC diagnostic pop
#endif

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
  void set_text(text_sensor::TextSensor *text) { this->text_ = text; }
#ifdef USE_LVGL
  void set_label(lv_obj_t *label) { this->label_ = label; }
#endif

 private:
  int64_t when_{-1};
  text_sensor::TextSensor *text_{nullptr};
#ifdef USE_LVGL
  lv_obj_t *label_{nullptr};
#endif
};

}  // namespace _since
}  // namespace esphome