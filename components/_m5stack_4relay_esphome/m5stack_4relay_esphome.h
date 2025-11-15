#pragma once

#include <optional>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace _m5stack_4relay_esphome {

class Interface;

class Relay : public switch_::Switch {
  friend class Interface;

 public:
  Relay() = default;

  void set_interface(Interface *interface);
  void set_index(uint8_t index) { this->index_ = index; }

  void write_state(bool state) override;

 protected:
  Interface *interface_{nullptr};
  uint8_t index_{0};
};

class Interface : public Component, public i2c::I2CDevice {
 public:
  Interface() = default;

  void add(Relay *relay) { this->relays_.push_back(relay); }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  std::optional<bool> read_state(uint8_t index);
  bool write_state(uint8_t index, bool state);

 protected:
  std::vector<Relay *> relays_;

  std::optional<uint8_t> read_states();
  bool write_states(uint8_t states);
};

}  // namespace _m5stack_4relay_esphome
}  // namespace esphome
