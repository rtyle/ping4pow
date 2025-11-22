#pragma once

#include <functional>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <ping/ping_sock.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wc++11-compat"
#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/network/ip_address.h"
#pragma GCC diagnostic pop

#include "esphome/components/since_/since.hpp"

namespace esphome {
namespace ping_ {

class Ping;

class Target : public switch_::Switch {
  friend class Ping;

 public:
  Target();

  void set_ping(Ping *ping);
  void set_address(esphome::network::IPAddress address);
  void set_interval(uint32_t interval) { this->interval_ = interval; }
  void set_timeout(uint32_t timeout) { this->timeout_ = timeout; }

  void set_able(binary_sensor::BinarySensor *able);
  void set_since(since_::Since *since);

  void setup();

  void write_state(bool state) override;

 protected:
  Ping *ping_{nullptr};
  esphome::network::IPAddress address_{};
  uint32_t interval_{~0u};
  uint32_t timeout_{~0u};

  std::string tag_{};

  bool unpublished_{true};
  bool success_{false};
  int64_t when_{-1};

  binary_sensor::BinarySensor *able_{nullptr};
  since_::Since *since_{nullptr};

  void publish(bool success);

  esp_ping_handle_t session_{nullptr};
};

class Ping : public Component {
 public:
  Ping();

  void set_none(binary_sensor::BinarySensor *none);
  void set_some(binary_sensor::BinarySensor *some);
  void set_all(binary_sensor::BinarySensor *all);
  void set_count(sensor::Sensor *count);
  void set_since(since_::Since *since);

  void add(Target *target);

  float get_setup_priority() const override;
  void setup() override;
  void loop() override;
  void dump_config() override;

  bool enqueue(std::function<void()>);

  void publish();

 protected:
  binary_sensor::BinarySensor *none_{nullptr};
  binary_sensor::BinarySensor *some_{nullptr};
  binary_sensor::BinarySensor *all_{nullptr};
  sensor::Sensor *count_{nullptr};
  since_::Since *since_{nullptr};

  std::vector<Target *> targets_{};

  QueueHandle_t const queue_;
};

}  // namespace ping_
}  // namespace esphome
