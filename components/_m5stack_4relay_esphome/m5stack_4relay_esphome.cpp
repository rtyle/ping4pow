#include "m5stack_4relay_esphome.h"

#include "esphome/core/log.h"

namespace esphome {
namespace _m5stack_4relay_esphome {

static constexpr char const TAG[]{"_m5stack_4relay_esphome"};

static constexpr uint8_t REGISTER{0x10};
static constexpr uint8_t COUNT{4};
static constexpr uint8_t MASK{(1 << COUNT) - 1};

void Relay::set_interface(Interface *interface) {
  this->interface_ = interface;
  this->interface_->add(this);
}

void Relay::write_state(bool state) {
  if (!this->interface_->write_state(this->index_, state)) {
    ESP_LOGE(TAG, "relay %d write state %s failed", this->index_, state ? "ON" : "OFF");
  } else {
    ESP_LOGD(TAG, "relay %d publish state %s", this->index_, state ? "ON" : "OFF");
    this->publish_state(state);
  }
}

void Interface::setup() {
  ESP_LOGCONFIG(TAG, "setup");

  if (!this->read_states()) {
    ESP_LOGE(TAG, "read states failed");
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

float Interface::get_setup_priority() const { return setup_priority::HARDWARE; }

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
  uint8_t bit{1 << index};
  if (state) {
    write_states |= bit;
  } else {
    write_states &= ~bit;
  }

  return this->write_states(write_states);
}

std::optional<uint8_t> Interface::read_states() {
  auto states{this->read_byte(REGISTER)};
  if (!states) {
    ESP_LOGW(TAG, "read failed");
    return {};
  }
  return *states;
}

bool Interface::write_states(uint8_t write_states) {
  write_states &= MASK;

  if (!this->write_byte(REGISTER, write_states)) {
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

}  // namespace _m5stack_4relay_esphome
}  // namespace esphome
