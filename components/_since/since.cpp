#include "since.h"

#include <esphome/core/log.h>

#include <esphome/components/_format/format.h>

namespace esphome {
namespace _since {

static constexpr char const TAG[]{"_since"};

void Since::update() {
  // publish state in units of seconds.
  // in a 32 bit float (with a 24 bit mantissa)
  // we will not lose precision until 2**24 seconds (over 194 days).
  float state{0 > this->when_
    ? std::numeric_limits<float>::quiet_NaN()
    : static_cast<float>((esp_timer_get_time() - this->when_) / 1000000)
  };
  this->publish_state(state);

  // if there is a text or label associated with us ...
  if (this->text_ ||
#ifdef USE_LVGL
      this->label_
#else
      false
#endif
  ) {
    // ... format our state and forward it
    auto s{ _format::duration(state)};
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
