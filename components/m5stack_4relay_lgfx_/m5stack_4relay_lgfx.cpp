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

#include "m5stack_4relay_lgfx.h"

#include "esphome/core/log.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsuggest-override"
#include <M5Unified.h>
#pragma GCC diagnostic pop

#include <lgfx/v1/platforms/common.hpp>

namespace esphome {
namespace m5stack_4relay_lgfx_ {

namespace {

constexpr char const TAG[]{"m5stack_4relay_lgfx_"};

inline constexpr int PORT{I2C_NUM_1};
inline constexpr int ADDRESS{0x26};

inline constexpr uint8_t REGISTER{0x10};
inline constexpr uint8_t COUNT{4};
inline constexpr uint8_t MASK{(1 << COUNT) - 1};

}  // namespace

void Relay::set_interface(Interface *interface) {
  this->interface_ = interface;
  this->interface_->add(this);
}

void Relay::write_state(bool state_) {
  if (!this->interface_->write_state(this->index_, state_)) {
    ESP_LOGE(TAG, "relay %d write state %s failed", this->index_, state_ ? "ON" : "OFF");
  } else {
    ESP_LOGD(TAG, "relay %d publish state %s", this->index_, state_ ? "ON" : "OFF");
    this->publish_state(state_);
  }
}

Interface::Interface() : port_{PORT}, address_{ADDRESS} {}

void Interface::setup() {
  ESP_LOGCONFIG(TAG, "setup");

  if (!this->read_states()) {
    ESP_LOGE(TAG, "read states from i2c port %d address 0x%x failed", this->port_, this->address_);
    this->mark_failed("read states");
    return;
  }

  if (!this->write_states(0u)) {
    this->mark_failed("write states");
    return;
  }

  for (auto relay : this->relays_) {
    if (auto state{this->read_state(relay->index_)}) {
      ESP_LOGD(TAG, "relay %d publish state %s", relay->index_, *state ? "ON" : "OFF");
      relay->publish_state(*state);
    } else {
      ESP_LOGE(TAG, "relay %d read state failed", relay->index_);
    }
  }
}

void Interface::dump_config() {
  ESP_LOGCONFIG(TAG, "config");
  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "\tfailed");
    return;
  }

  for (auto relay : this->relays_) {
    ESP_LOGCONFIG(TAG, "\trelay: %d", relay->index_);
    switch_::log_switch(TAG, " ", "Switch", relay);
  }
}

float Interface::get_setup_priority() const { return setup_priority::DATA; }

std::optional<bool> Interface::read_state(uint8_t index) {
  if (!(index < COUNT)) {
    ESP_LOGW(TAG, "invalid relay %d", index);
    return {};
  }

  auto states{this->read_states()};
  if (!states) {
    return {};
  }

  return 0 != (*states & (1 << index));
}

bool Interface::write_state(uint8_t index, bool state) {
  if (!(index < COUNT)) {
    ESP_LOGW(TAG, "invalid relay %d", index);
    return false;
  }

  auto read_states{this->read_states()};
  if (!read_states) {
    return false;
  }

  uint8_t write_states{*read_states};
  uint8_t bit{static_cast<uint8_t>(1 << index)};
  if (state) {
    write_states |= bit;
  } else {
    write_states &= ~bit;
  }

  return this->write_states(write_states);
}

std::optional<uint8_t> Interface::read_states() {
  auto states{lgfx::i2c::readRegister8(this->port_, this->address_, REGISTER)};
  if (states.has_error()) {
    ESP_LOGW(TAG, "read failed: error %d", states.error());
    return {};
  }
  return states.value();
}

bool Interface::write_states(uint8_t write_states) {
  write_states &= MASK;

  if (!lgfx::i2c::writeRegister8(this->port_, this->address_, REGISTER, write_states)) {
    ESP_LOGW(TAG, "write 0x%x failed", write_states);
    return false;
  }

  auto read_states{this->read_states()};
  if (!read_states) {
    return false;
  }

  if (write_states != *read_states) {
    ESP_LOGW(TAG, "write 0x%x failed, read 0x%x", write_states, *read_states);
    return false;
  }

  ESP_LOGD(TAG, "write 0x%x", write_states);
  return true;
}

}  // namespace m5stack_4relay_lgfx_
}  // namespace esphome

#pragma GCC diagnostic pop