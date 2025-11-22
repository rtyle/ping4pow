#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#pragma GCC diagnostic pop

namespace esphome {
namespace m5cores3 {

class M5CoreS3Touchscreen : public touchscreen::Touchscreen {
 public:
  void setup() override;
  void dump_config() override;

  void update_touches() override;

  void set_left(binary_sensor::BinarySensor *);
  void set_center(binary_sensor::BinarySensor *);
  void set_right(binary_sensor::BinarySensor *);

 protected:
  binary_sensor::BinarySensor **from_{nullptr};
  binary_sensor::BinarySensor *left_{nullptr};
  binary_sensor::BinarySensor *center_{nullptr};
  binary_sensor::BinarySensor *right_{nullptr};
};

}  // namespace m5cores3
}  // namespace esphome
