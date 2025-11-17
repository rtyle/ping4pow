#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wall"
#pragma GCC diagnostic warning "-Wextra"
#pragma GCC diagnostic warning "-Wpedantic"
#pragma GCC diagnostic warning "-Wconversion"
#pragma GCC diagnostic warning "-Wsign-conversion"
#pragma GCC diagnostic warning "-Wold-style-cast"
#pragma GCC diagnostic warning "-Wshadow"
#pragma GCC diagnostic warning "-Wnull-dereference"
#pragma GCC diagnostic warning "-Wformat=2"
#pragma GCC diagnostic warning "-Wsuggest-override"
#pragma GCC diagnostic warning "-Wzero-as-null-pointer-constant"

#include "since.h"

#include <esphome/core/log.h>

#include <esphome/components/_format/format.h>

namespace esphome {
namespace _since {

namespace {

constexpr char const TAG[]{"_since"};

}   // namespace /* unnamed */

void Since::update() {
  // publish state in units of seconds.
  // in a 32 bit float (with a 24 bit mantissa)
  // we will not lose precision until 2**24 seconds (over 194 days).
  float state_{0 > this->when_ ? std::numeric_limits<float>::quiet_NaN()
                               : static_cast<float>((esp_timer_get_time() - this->when_) / 1000000)};
  this->publish_state(state_);

  // if there is a text or label associated with us ...
  if (this->text_ ||
#ifdef USE_LVGL
      this->label_
#else
      false
#endif
  ) {
    // ... format our state and forward it
    auto s{_format::duration(state_)};
    if (this->text_) {
      this->text_->publish_state(s);
    }
#ifdef USE_LVGL
    if (this->label_) {
      lv_label_set_text(this->label_, s.c_str());
      lv_event_send(this->label_, lvgl::lv_update_event, nullptr);
    }
#endif
  }
}

void Since::set_when(int64_t when) {
  this->when_ = when;
  this->update();
}

}  // namespace _since
}  // namespace esphome

#pragma GCC diagnostic pop