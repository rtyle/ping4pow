#include "since.h"

#include <esphome/core/log.h>

namespace esphome {
namespace _since {

static const char *const TAG = "_since";

void Since::update() {
  // publish state in units of seconds.
  // in a 32 bit float (with a 24 bit mantissa)
  // we will not lose precision until 2**24 seconds (over 194 days).
  this->publish_state(0 > this->when_ ? std::numeric_limits<float>::quiet_NaN()
                                      : static_cast<float>((esp_timer_get_time() - this->when_) / 1000000));
}

void Since::set_when(int64_t when) {
  this->when_ = when;
  this->update();
}

}  // namespace _since
}  // namespace esphome
