#include "m5cores3_touchscreen.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <M5Unified.h>

namespace esphome {
namespace m5cores3 {

static const char *const TAG = "m5cores3.touchscreen";

void M5CoreS3Touchscreen::setup() {
  esph_log_config(TAG, "Setting up M5CoreS3 Touchscreen...");
  this->x_raw_max_ = M5.Display.width();
  this->y_raw_max_ = M5.Display.height();
  esph_log_config(TAG, "M5CoreS3 Touchscreen setup complete");
}

void M5CoreS3Touchscreen::dump_config() { esph_log_config(TAG, "M5CoreS3 Touchscreen:"); }

void M5CoreS3Touchscreen::update_touches() {
  M5.update();

  auto t = M5.Touch.getDetail();

  if ((t.state & m5::touch_state_t::mask_touch) && (t.state & m5::touch_state_t::mask_change)) {
    // this is the beginning of a new gesture.
    // if the last gesture started from the bezel, publish_state(false) and forget it.
    if (this->from_ && *this->from_) {
      (*this->from_)->publish_state(false);
    }
    this->from_ = nullptr;
    if (t.y >= this->y_raw_max_) {
      // the new gesture has started on the bezel
      // find out where and publish_state accordingly.
      switch (3 * t.x / this->x_raw_max_) {
        case 0:
          this->from_ = &this->left_;
          break;
        case 1:
          this->from_ = &this->center_;
          break;
        case 2:
          this->from_ = &this->right_;
          break;
      }
      if (*this->from_) {
        (*this->from_)->publish_state(true);
      }
      // swallow this and future events from this gesture
      return;
    }
  }

  // swallow events that started from the bezel until touch isReleased
  if (this->from_) {
    if (t.isReleased()) {
      if (this->from_) {
        (*this->from_)->publish_state(false);
      }
      this->from_ = nullptr;
    }
    return;
  }

  // only forward wasPressed event
  if (t.wasPressed()) {
    this->add_raw_touch_position_(1, t.x, t.y);
  }
}

void M5CoreS3Touchscreen::set_left(binary_sensor::BinarySensor *left) { this->left_ = left; }
void M5CoreS3Touchscreen::set_center(binary_sensor::BinarySensor *center) { this->center_ = center; }
void M5CoreS3Touchscreen::set_right(binary_sensor::BinarySensor *right) { this->right_ = right; }

}  // namespace m5cores3
}  // namespace esphome
