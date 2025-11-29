#pragma once

#include <functional>
#include <optional>
#include <string>

#include "freertos/FreeRTOS.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
// #pragma GCC diagnostic ignored -Wpedantic
// should squelch impending
//  warning: #include_next is a GCC extension
// messages, but does not.
// these are squelched if -Wpedantic was never turned on.
// this is a bug!
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#pragma GCC diagnostic pop

namespace esphome {
namespace smtp_ {

#include "raii.hpp"

class Component : public esphome::Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::ETHERNET; }

  // configuration setters
  void set_server(std::string const &value) { this->server_ = value; }
  void set_port(uint16_t const value) { this->port_ = value; }
  void set_username(std::string const &value) { this->username_ = value; }
  void set_password(std::string const &value) { this->password_ = value; }
  void set_from(std::string const &value) { this->from_ = value; }
  void set_to(std::string const &value) { this->to_ = value; }
  void set_starttls(bool const value) { this->starttls_ = value; }
  void set_cas(std::string const &value) { this->cas_ = value; }
  void set_task_name(std::string const &value) { this->task_name_ = value; }
  void set_task_priority(unsigned const value) { this->task_priority_ = value; }

  void enqueue(std::string const &subject, std::string const &body, std::string const &to = "");

 private:
  struct Message {
    std::string subject;
    std::string body;
    std::string to;
  };

  // use RAII to manage mbedtls resources for our lifetime
  raii::Resource<mbedtls_entropy_context> entropy_{raii::make(mbedtls_entropy_init, mbedtls_entropy_free)};
  raii::Resource<mbedtls_ctr_drbg_context> ctr_drbg_{raii::make(mbedtls_ctr_drbg_init, mbedtls_ctr_drbg_free)};
  raii::Resource<mbedtls_x509_crt> x509_crt_{raii::make(mbedtls_x509_crt_init, mbedtls_x509_crt_free)};
  raii::Resource<mbedtls_ssl_config> ssl_config_{raii::make(mbedtls_ssl_config_init, mbedtls_ssl_config_free)};

  void run_();
  std::optional<std::string> send_(std::function<std::unique_ptr<Message>()>);

  TaskHandle_t task_handle_{nullptr};
  QueueHandle_t queue_{nullptr};

  // configuration
  std::string server_;
  uint16_t port_{587};
  std::string username_;
  std::string password_;
  std::string from_;
  std::string to_;
  bool starttls_{true};
  std::string cas_;
  std::string task_name_{"smtp"};
  unsigned task_priority_{5};
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