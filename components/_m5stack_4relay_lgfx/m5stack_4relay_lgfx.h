#pragma once

#include <optional>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wshadow"
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#pragma GCC diagnostic pop

namespace esphome {
namespace _m5stack_4relay_lgfx {

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

class Interface : public Component {
 public:
  Interface();

  void set_port(int port) { this->port_ = port; }
  void set_address(int address) { this->address_ = address; }

  void add(Relay *relay) { this->relays_.push_back(relay); }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  std::optional<bool> read_state(uint8_t index);
  bool write_state(uint8_t index, bool state);

 protected:
  int port_;
  int address_;
  std::vector<Relay *> relays_;

  std::optional<uint8_t> read_states();
  bool write_states(uint8_t states);
};

}  // namespace _m5stack_4relay_lgfx
}  // namespace esphome