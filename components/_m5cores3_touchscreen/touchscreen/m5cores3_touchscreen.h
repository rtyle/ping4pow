#pragma once

#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"

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
