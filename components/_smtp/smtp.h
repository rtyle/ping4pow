#pragma once

#include <functional>
#include <optional>
#include <string>

#include "freertos/FreeRTOS.h"

#include "esphome/core/component.h"
#include "esphome/core/automation.h"

#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"

namespace esphome {
namespace _smtp {

#include "raii.hpp"

struct Message {
  std::string subject;
  std::string body;
  std::string to;
};

class Component : public esphome::Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::ETHERNET; }

  // configuration setters
  void set_server(const std::string &value) { this->server_ = value; }
  void set_port(uint16_t value) { this->port_ = value; }
  void set_username(const std::string &value) { this->username_ = value; }
  void set_password(const std::string &value) { this->password_ = value; }
  void set_from(const std::string &value) { this->from_ = value; }
  void set_to(const std::string &value) { this->to_ = value; }
  void set_starttls(bool value) { this->starttls_ = value; }
  void set_cas(const std::string &value) { this->cas_ = value; }

  void enqueue(const std::string &subject, const std::string &body, const std::string &to = "");

 protected:
  // use RAII to manage mbedtls resources for our lifetime
  raii::Resource<mbedtls_entropy_context> entropy_{raii::make(mbedtls_entropy_init, mbedtls_entropy_free)};
  raii::Resource<mbedtls_ctr_drbg_context> ctr_drbg_{raii::make(mbedtls_ctr_drbg_init, mbedtls_ctr_drbg_free)};
  raii::Resource<mbedtls_x509_crt> x509_crt_{raii::make(mbedtls_x509_crt_init, mbedtls_x509_crt_free)};
  raii::Resource<mbedtls_ssl_config> ssl_config_{raii::make(mbedtls_ssl_config_init, mbedtls_ssl_config_free)};

  static void run_that_(void *);
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
  std::string cas_;
  bool starttls_{true};
};

// Action for sending emails
template<typename... Ts> class Action : public esphome::Action<Ts...> {
 public:
  explicit Action(Component *parent) : parent_{parent} {}

  TEMPLATABLE_VALUE(std::string, subject)
  TEMPLATABLE_VALUE(std::string, body)
  TEMPLATABLE_VALUE(std::string, to)

  void play(Ts... x) override {
    auto subject{this->subject_.value(x...)};
    auto body{this->body_.optional_value(x...).value_or("")};
    auto to{this->to_.optional_value(x...).value_or("")};
    this->parent_->enqueue(subject, body, to);
  }

 protected:
  Component *parent_;
};

}  // namespace _smtp
}  // namespace esphome