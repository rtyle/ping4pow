#include "ping.h"

#include <esp_timer.h>

namespace esphome {
namespace _ping {

static const char *const TAG = "_ping";

Target::Target() {}

void Target::set_ping(Ping *ping) {
  this->ping_ = ping;
  ping->add(this);
}

void Target::set_address(esphome::network::IPAddress address) {
  this->address_ = address;
  this->tag_ = this->get_name() + " " + this->address_.str().c_str();
}

void Target::set_able(binary_sensor::BinarySensor *able) {
  this->able_ = able;
  this->able_->publish_state(false);
}

void Target::set_since(_since::Since *since) {
  this->since_ = since;
  this->since_->update();
}

void Target::publish(bool success) {
  this->unpublished_ = false;
  if (success) {
    ESP_LOGD(TAG, "%s ping success", this->tag_.c_str());
  } else {
    ESP_LOGW(TAG, "%s ping failure", this->tag_.c_str());
  }
  if (this->success_ != success) {
    ESP_LOGD(TAG, "%s ping %s", this->tag_.c_str(), success ? "failure→success" : "success→failure");
    this->success_ = success;
    this->when_ = esp_timer_get_time();
    if (this->able_)
      this->able_->publish_state(this->success_);
    if (this->since_)
      this->since_->set_when(this->when_);
    this->ping_->publish();
  }
}

void Target::setup() {
  esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
  config.count = ESP_PING_COUNT_INFINITE;
  config.interval_ms = this->interval_;
  config.timeout_ms = this->timeout_;
  config.target_addr = this->address_;
  // all callbacks occur in the context of the ping session task/thread.
  // esphome responses should be done in the esphome task/thread.
  // all call backs simply defer these responses
  // so there is no need to enlarge the ping task's stack
  // and the responses are done in thread safe manner.
  esp_ping_callbacks_t callbacks{
      .cb_args = this,
      .on_ping_success =
          [](esp_ping_handle_t, void *target) {
            Target *target_{reinterpret_cast<Target *>(target)};
            target_->ping_->enqueue([target_]() { target_->publish(true); });
          },
      .on_ping_timeout =
          [](esp_ping_handle_t, void *target) {
            Target *target_{reinterpret_cast<Target *>(target)};
            target_->ping_->enqueue([target_]() { target_->publish(false); });
          },
      .on_ping_end =
          [](esp_ping_handle_t, void *target) {
            Target *target_{reinterpret_cast<Target *>(target)};
            target_->ping_->enqueue([target_]() {
              target_->publish_state(false);
              target_->ping_->publish();
            });
          },
  };
  ESP_LOGD(TAG,
           "%s ping session {count=%d interval=%d timeout=%d size=%d tos=%d ttl=%d address=%s stack=%d "
           "priority=%d, interface=%d}",
           this->tag_.c_str(), config.count, config.interval_ms, config.timeout_ms, config.data_size, config.tos,
           config.ttl, this->address_.str().c_str(), config.task_stack_size, config.task_prio, config.interface);
  auto result = esp_ping_new_session(&config, &callbacks, &this->session_);
  if (ESP_OK != result) {
    ESP_LOGE(TAG, "%s ping session failed: %s", this->tag_.c_str(), esp_err_to_name(result));
  } else {
    this->write_state(true);
  }
}

void Target::write_state(bool state) {
  if (this->session_) {
    if (state) {
      ESP_LOGD(TAG, "%s ping start", this->tag_.c_str());
      auto result = esp_ping_start(this->session_);
      if (ESP_OK == result) {
        this->publish_state(true);
      } else {
        this->publish_state(false);
        ESP_LOGE(TAG, "%s ping start failed: %s", this->tag_.c_str(), esp_err_to_name(result));
      }
      this->ping_->publish();
    } else {
      ESP_LOGD(TAG, "%s ping stop", this->tag_.c_str());
      auto result = esp_ping_stop(this->session_);
      if (ESP_OK != result) {
        ESP_LOGE(TAG, "%s ping stop failed: %s", this->tag_.c_str(), esp_err_to_name(result));
      }
      // publish on_ping_end
    }
  }
}

Ping::Ping() : queue_{xQueueCreate(32, sizeof(std::function<void()> *))} {}

void Ping::add(Target *target) { this->targets_.push_back(target); }

float Ping::get_setup_priority() const { return esphome::setup_priority::AFTER_CONNECTION; }

void Ping::setup() {
  ESP_LOGCONFIG(TAG, "setup");
  for (auto target : this->targets_) {
    target->setup();
  }
}

bool Ping::enqueue(std::function<void()> f) {
  auto *copy = new std::function<void()>(std::move(f));
  if (pdPASS == xQueueSend(this->queue_, &copy, portMAX_DELAY)) {
    return true;
  }
  ESP_LOGE(TAG, "enqueue failed");
  delete copy;
  return false;
}

void Ping::loop() {
  // all esphome code should run in its (this) thread.
  // run such deferred code now.
  std::function<void()> *f;
  while (pdTRUE == xQueueReceive(queue_, &f, 0)) {
    (*f)();
    delete f;
  }
}

void Ping::dump_config() {
  ESP_LOGCONFIG(TAG, "ping:");
  LOG_BINARY_SENSOR(TAG, "all", this->all_);
  LOG_BINARY_SENSOR(TAG, "none", this->none_);
  for (auto *target : this->targets_) {
    ESP_LOGCONFIG(TAG, "target '%s':", target->get_name());
    ESP_LOGCONFIG(TAG, "address: %s", target->address_.str().c_str());
    ESP_LOGCONFIG(TAG, "timeout: %d ms", target->timeout_);
    ESP_LOGCONFIG(TAG, "interval: %d ms", target->interval_);
    if (target->able_)
      LOG_BINARY_SENSOR(TAG, "able", target->able_);
    if (target->since_)
      LOG_SENSOR(TAG, "since", target->since_);
  }
}

void Ping::set_none(binary_sensor::BinarySensor *none) {
  this->none_ = none;
  this->none_->publish_state(false);
}
void Ping::set_some(binary_sensor::BinarySensor *some) {
  this->some_ = some;
  this->some_->publish_state(false);
}
void Ping::set_all(binary_sensor::BinarySensor *all) {
  this->all_ = all;
  this->all_->publish_state(false);
}
void Ping::set_count(sensor::Sensor *count) {
  this->count_ = count;
  this->count_->publish_state(0);
}
void Ping::set_since(_since::Since *since) {
  this->since_ = since;
  this->since_->update();
}

void Ping::publish() {
  bool unpublished{false};  // until an enabled target is unpublished
  bool none{true};          // until an enabled target is successful
  bool all{true};           // until an enabled target is not successful
  size_t count{0};          // count of enabled successful targets
  int64_t latest{-1};       // latest on target
  for (auto *target : this->targets_) {
    if (target->state) {  // on
      if (target->unpublished_) {
        unpublished = true;
      }
      if (target->success_) {
        none = false;
        ++count;
      } else {
        all = false;
      }
      if (latest < target->when_) {
        latest = target->when_;
      }
    }
  }
  if (all || unpublished)
    none = false;
  bool some{!all && !none};

  if (this->none_)
    this->none_->publish_state(none);
  if (this->some_)
    this->some_->publish_state(some);
  if (this->all_)
    this->all_->publish_state(all);
  if (this->count_)
    this->count_->publish_state(count);
  if (this->since_)
    this->since_->set_when(latest);
}

}  // namespace _ping
}  // namespace esphome
