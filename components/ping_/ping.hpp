#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsuggest-override"
#pragma GCC diagnostic ignored "-Wc++11-compat"
#include <asio/io_context.hpp>
#include <asio/ip/icmp.hpp>
#include <asio/steady_timer.hpp>
#pragma GCC diagnostic pop

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
  void set_interval(int64_t const interval) {
    this->interval_ = asio::steady_timer::duration(std::chrono::nanoseconds(interval));
  }
  void set_timeout(int64_t const timeout) {
    this->timeout_ = asio::steady_timer::duration(std::chrono::nanoseconds(timeout));
  }

  void set_able(binary_sensor::BinarySensor *able);
  void set_since(since_::Since *since);

  void setup(std::size_t index, std::size_t size);

  void write_state(bool state) override;

 private:
  Ping *ping_{nullptr};
  asio::ip::icmp::endpoint endpoint_{};
  asio::steady_timer::duration interval_{};
  asio::steady_timer::duration timeout_{};

  std::string tag_{};

  bool unpublished_{true};
  bool success_{false};
  asio::steady_timer::time_point reply_timepoint_{asio::steady_timer::time_point::min()};
  asio::steady_timer::time_point change_timepoint_{asio::steady_timer::time_point::min()};
  std::unique_ptr<asio::steady_timer> timer_{};

  binary_sensor::BinarySensor *able_{nullptr};
  since_::Since *since_{nullptr};

  void publish(bool success, asio::steady_timer::time_point const &timepoint);

  void reply(asio::ip::icmp::endpoint const &endpoint, uint16_t sequence,
             asio::steady_timer::time_point const &timepoint);
};

class Ping : public Component {
  friend class Target;

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
  bool teardown() override;
  void loop() override;
  void dump_config() override;

  void publish();

 private:
  binary_sensor::BinarySensor *none_{nullptr};
  binary_sensor::BinarySensor *some_{nullptr};
  binary_sensor::BinarySensor *all_{nullptr};
  sensor::Sensor *count_{nullptr};
  since_::Since *since_{nullptr};

  std::vector<Target *> targets_{};

  asio::io_context io_{};
  std::unique_ptr<asio::ip::icmp::socket> socket_{};
};

}  // namespace ping_
}  // namespace esphome
