#pragma once

#include <deque>
#include <optional>
#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsuggest-override"
#pragma GCC diagnostic ignored "-Wc++11-compat"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ssl/stream.hpp>
#include <asio/steady_timer.hpp>
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#pragma GCC diagnostic pop

namespace esphome {
namespace smtp_ {

class Component : public esphome::Component {
 public:
  explicit Component();

  void dump_config() override;

  // lifecycle
  float get_setup_priority() const override;
  void setup() override;
  bool teardown() override;
  void loop() override;

  // configuration setters
  void set_server(std::string const &value) { this->server_ = value; }
  void set_port(uint16_t const value) { this->port_ = value; }
  void set_username(std::string const &value) { this->username_ = value; }
  void set_password(std::string const &value) { this->password_ = value; }
  void set_from(std::string const &value) { this->from_ = value; }
  void set_to(std::string const &value) { this->to_ = value; }
  void set_starttls(bool const value) { this->starttls_ = value; }
  void set_cas(std::string const &value) { this->cas_ = value; }

  void enqueue(std::string const &subject, std::string const &body, std::string const &to = "");

 private:
  struct Message {
    std::string subject;
    std::string body;
    std::string to;
  };

  // configuration
  std::string server_;
  uint16_t port_;
  std::string username_;
  std::string password_;
  std::string from_;
  std::string to_;
  bool starttls_;
  std::string cas_;

  asio::io_context io_;
  std::deque<Message> queue_;

  std::optional<asio::steady_timer> queue_timer_;
  std::optional<asio::steady_timer> interval_timer_;
  asio::ssl::context ssl_;
  std::optional<asio::ssl::stream<asio::ip::tcp::socket>> stream_;
};

// Action for sending emails
template<typename... Ts> class Action : public esphome::Action<Ts...> {
 public:
  explicit Action(Component *const parent) : parent_{parent} {}

  TEMPLATABLE_VALUE(std::string, subject)
  TEMPLATABLE_VALUE(std::string, body)
  TEMPLATABLE_VALUE(std::string, to)

  void play(Ts... x) override {
    auto const subject{this->subject_.value(x...)};
    auto const body{this->body_.optional_value(x...).value_or("")};
    auto const to{this->to_.optional_value(x...).value_or("")};
    this->parent_->enqueue(subject, body, to);
  }

 protected:
  Component *parent_;
};

}  // namespace smtp_
}  // namespace esphome