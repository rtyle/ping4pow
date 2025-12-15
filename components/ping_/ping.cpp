#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic error "-Wpedantic"
#pragma GCC diagnostic error "-Wconversion"
#pragma GCC diagnostic error "-Wsign-conversion"
#pragma GCC diagnostic error "-Wold-style-cast"
#pragma GCC diagnostic error "-Wshadow"
#pragma GCC diagnostic error "-Wnull-dereference"
#pragma GCC diagnostic error "-Wformat=2"
#pragma GCC diagnostic error "-Wsuggest-override"
#pragma GCC diagnostic error "-Wzero-as-null-pointer-constant"

#include "ping.hpp"

#include <ranges>
#include <span>

// provide code generated from asio includes that follow below
// visibility to our ASIO_NO_EXCEPTIONS asio::detail::throw_exception definition,
// if called for, so that they can implicitly instantiate what they need.
#include "esphome/components/asio_/asio_detail_throw_exception_.cpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wsuggest-override"
#pragma GCC diagnostic ignored "-Wc++11-compat"
#include <asio/buffer.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/redirect_error.hpp>
#include <asio/this_coro.hpp>
#pragma GCC diagnostic pop

namespace esphome {
namespace ping_ {

namespace {

constexpr auto TAG{"ping_"};

#pragma pack(push, 1)

class Timestamp {
 private:
  std::int64_t as_nanoseconds_{};

 public:
  Timestamp() = default;
  Timestamp(asio::steady_timer::time_point const timepoint)
      : as_nanoseconds_{std::chrono::time_point_cast<std::chrono::nanoseconds>(timepoint).time_since_epoch().count()} {}
  operator std::int64_t() const { return this->as_nanoseconds_; }
  operator asio::steady_timer::time_point() const {
    return asio::steady_timer::time_point(std::chrono::nanoseconds(this->as_nanoseconds_));
  }
};

constexpr auto IP_HEADER_SIZE_MIN{20};
constexpr auto IP_HEADER_SIZE_MAX{60};
constexpr auto PADDING_SIZE{48};
constexpr auto PACKET_SIZE{16 + PADDING_SIZE};

constexpr auto PADDING{[]() consteval {
  std::array<std::byte, PADDING_SIZE> pattern{};
  pattern.fill(std::byte{0x5A});
  return pattern;
}()};

class Packet {
 private:
  std::byte type_;
  std::byte code_;
  std::uint16_t checksum_;
  std::uint16_t id_;
  std::uint16_t sequence_;
  Timestamp timestamp_;
  std::array<std::byte, PADDING_SIZE> padding_;

  std::uint16_t checksum_compute() const {
    std::uint32_t sum{0};
    for (auto addend :
         std::span{reinterpret_cast<std::uint16_t const *>(this), this->size() / sizeof(std::uint16_t)} |
             std::views::transform([](auto const word) { return static_cast<std::uint32_t>(ntohs(word)); })) {
      sum += addend;
    }
    // add carries into high word back into low word
    while (sum >> 16) {
      sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return static_cast<std::uint16_t>(~sum);
  }
  bool checksum_check() const { return 0 == this->checksum_compute(); }
  bool padding_check() const { return this->padding_ == PADDING; }

 public:
  Packet(std::uint16_t const id, std::uint16_t sequence, asio::steady_timer::time_point const &timepoint)
      : type_{8},  // echo request
        code_{0},
        checksum_{0},
        id_{htons(id)},
        sequence_{htons(sequence)},
        timestamp_{timepoint},
        padding_{PADDING} {
    this->checksum_ = htons(checksum_compute());
  }

  std::byte type() const { return this->type_; }
  std::byte code() const { return this->code_; }
  std::uint16_t checksum() const { return ntohs(this->checksum_); }
  std::uint16_t id() const { return ntohs(this->id_); }
  std::uint16_t sequence() const { return ntohs(this->sequence_); }
  asio::steady_timer::time_point timepoint() const { return {this->timestamp_}; }

  void const *data() const { return reinterpret_cast<void const *>(this); }
  std::size_t size() const { return sizeof(*this); }

  static bool fits(std::span<std::byte const> const from) { return sizeof(Packet) == from.size(); }
  Packet(std::span<std::byte const> const from) {
    std::copy_n(from.begin(), sizeof(*this), reinterpret_cast<std::byte *>(this));  // echo reply?
  }
  bool is_reply() const { return this->checksum_check() && this->padding_check() && std::byte{0} == this->type_; }
};
static_assert(PACKET_SIZE == sizeof(Packet), "Packet not packed properly");

#pragma pack(pop)

}  // namespace

Target::Target() = default;

void Target::set_ping(Ping *const ping) {
  this->ping_ = ping;
  ping->add(this);
}

void Target::set_address(esphome::network::IPAddress const address) {
  // asio::ip::address_v4 takes address in host byte order!?
  this->endpoint_.address(asio::ip::address_v4(ntohl(static_cast<ip_addr_t>(address).u_addr.ip4.addr)));
  this->tag_ = std::string(this->get_name()) + ' ' + this->endpoint_.address().to_string();
}

void Target::set_able(binary_sensor::BinarySensor *const able) {
  this->able_ = able;
  this->able_->publish_state(false);
}

void Target::set_since(since_::Since *const since) {
  this->since_ = since;
  this->since_->update();
}

void Target::publish(bool const success, asio::steady_timer::time_point const &timepoint) {
  this->unpublished_ = false;
  if (success) {
    ESP_LOGD(TAG, "%s ping success", this->tag_.c_str());
  } else {
    ESP_LOGW(TAG, "%s ping failure", this->tag_.c_str());
  }
  if (this->success_ != success) {
    ESP_LOGI(TAG, "%s ping %s", this->tag_.c_str(), success ? "failure→success" : "success→failure");
    this->success_ = success;
    this->change_timepoint_ = timepoint;
    if (this->able_)
      this->able_->publish_state(this->success_);
    if (this->since_)
      this->since_->set_when(timepoint);
    this->ping_->publish();
  }
}

void Target::setup(std::size_t const index, std::size_t const size) {
  this->timer_ = std::make_unique<asio::steady_timer>(this->ping_->io_);

  this->write_state(true);

  // periodically send ICMP echo requests to this->endpoint_
  asio::co_spawn(
      this->ping_->io_,
      [this, index, size]() -> asio::awaitable<void> {
        auto const id{static_cast<std::uint16_t>(index)};
        std::uint16_t sequence{0};
        // stagger start in an attempt to be out of phase with other targets
        this->timer_->expires_after(this->interval_ * index / size);
        while (true) {
          {
            std::error_code ec;
            co_await this->timer_->async_wait(asio::redirect_error(asio::use_awaitable, ec));
            if (ec == asio::error::operation_aborted) {
              ESP_LOGD(TAG, "%s abort: timer %s", this->tag_.c_str(), ec.message().c_str());
              break;
            } else if (ec) {
              ESP_LOGW(TAG, "%s timer error: %s", this->tag_.c_str(), ec.message().c_str());
              continue;
            }
          }
          auto const request_timepoint{this->timer_->expiry()};
          if (this->state) {
            bool teardown{false};
            do {
              Packet const packet{id, sequence++, request_timepoint};
              ESP_LOGD(TAG, "%s sending ICMP echo request", this->tag_.c_str());
              {
                std::error_code ec;
                co_await this->ping_->socket_->async_send_to(asio::const_buffer(packet.data(), packet.size()),
                                                             this->endpoint_,
                                                             asio::redirect_error(asio::use_awaitable, ec));
                if (ec == asio::error::operation_aborted) {
                  ESP_LOGD(TAG, "%s abort: send_to %s", this->tag_.c_str(), ec.message().c_str());
                  teardown = true;
                  break;
                } else if (ec) {
                  ESP_LOGW(TAG, "%s send_to error: %s", this->tag_.c_str(), ec.message().c_str());
                  break;
                }
              }
              this->timer_->expires_at(request_timepoint + this->timeout_);
              {
                std::error_code ec;
                co_await this->timer_->async_wait(asio::redirect_error(asio::use_awaitable, ec));
                if (ec == asio::error::operation_aborted) {
                  ESP_LOGD(TAG, "%s abort: timer %s", this->tag_.c_str(), ec.message().c_str());
                  teardown = true;  // teardown
                  break;
                } else if (ec) {
                  ESP_LOGW(TAG, "%s timer error: %s", this->tag_.c_str(), ec.message().c_str());
                  break;
                }
              }
              if (this->reply_timepoint_ != request_timepoint) {
                this->publish(false, request_timepoint);
              }
            } while (false);
            if (teardown)
              break;
          }
          // strictly periodic from now on
          this->timer_->expires_at(request_timepoint + this->interval_);
        }
        ESP_LOGD(TAG, "%s abort: timer reset", this->tag_.c_str());
        this->timer_.reset();
        co_return;
      },
      asio::detached);
}

void Target::reply(asio::ip::icmp::endpoint const &endpoint, uint16_t sequence,
                   asio::steady_timer::time_point const &timepoint) {
  // replies may come out of order, this->reply_timepoint_ must monotonically increase
  if (this->reply_timepoint_ < timepoint) {
    this->reply_timepoint_ = timepoint;
  }
  auto const round_trip_time{
      std::chrono::duration_cast<std::chrono::milliseconds>(asio::steady_timer::clock_type::now() - timepoint).count()};
  ESP_LOGD(TAG, "%s reply endpoint=%s sequence=%d rtt=%llu ms", this->tag_.c_str(),
           endpoint.address().to_string().c_str(), sequence, round_trip_time);
  this->publish(true, timepoint);
}

void Target::write_state(bool const state_) {
  ESP_LOGD(TAG, "%s ping %s", this->tag_.c_str(), state_ ? "start" : "stop");
  this->publish_state(state_);
  this->ping_->publish();
}

Ping::Ping() {}

void Ping::add(Target *const target) { this->targets_.push_back(target); }

void Ping::dump_config() {
  ESP_LOGCONFIG(TAG, "ping:");
  LOG_BINARY_SENSOR(TAG, "all", this->all_);
  LOG_BINARY_SENSOR(TAG, "none", this->none_);
  for (auto const *const target : this->targets_) {
    ESP_LOGCONFIG(TAG, "target '%s':", target->get_name());
    ESP_LOGCONFIG(TAG, "address: %s", target->endpoint_.address().to_string().c_str());
    ESP_LOGCONFIG(TAG, "timeout: %d ms", target->timeout_);
    ESP_LOGCONFIG(TAG, "interval: %d ms", target->interval_);
    if (target->able_)
      LOG_BINARY_SENSOR(TAG, "able", target->able_);
    if (target->since_)
      LOG_SENSOR(TAG, "since", target->since_);
  }
}

float Ping::get_setup_priority() const { return esphome::setup_priority::AFTER_CONNECTION; }

void Ping::setup() {
  ESP_LOGD(TAG, "setup");

  // we must delay socket creation until now (AFTER_CONNECTION)
  this->socket_ = std::make_unique<asio::ip::icmp::socket>(this->io_);

  {
    std::error_code ec;
    this->socket_->open(asio::ip::icmp::v4(), ec);
    if (ec) {
      ESP_LOGE(TAG, "socket open error: %s", ec.message().c_str());
      this->mark_failed();
      return;
    }
  }

  // setup each target with its index into targets_ and targets_.size().
  // it will use index to set the id in each ICMP request packet it sends,
  // which we will use to dispatch a matching reply back to the target.
  // it will use index / size to calculate a phase offset in its periodic requests.
  {
    size_t index{0};
    for (auto &target : this->targets_) {
      target->setup(index++, this->targets_.size());
    }
  }

  asio::co_spawn(
      this->io_,
      [this]() -> asio::awaitable<void> {
        std::error_code ec;
        while (true) {
          std::array<std::byte, IP_HEADER_SIZE_MAX + PACKET_SIZE> into{};  // receive into, at most, this
          asio::ip::icmp::endpoint endpoint{};
          auto const received{co_await this->socket_->async_receive_from(
              asio::mutable_buffer(into.data(), into.size()), endpoint, asio::redirect_error(asio::use_awaitable, ec))};
          if (ec == asio::error::operation_aborted) {
            ESP_LOGD(TAG, "abort: receive_from %s", ec.message().c_str());
            break;  // teardown
          } else if (ec) {
            ESP_LOGW(TAG, "receive_from error: %s", ec.message().c_str());
            continue;
          }
          if (received < IP_HEADER_SIZE_MIN + PACKET_SIZE) {
            ESP_LOGW(TAG, "received runt reply (%zu) bytes", received);
            continue;
          }
          std::span<std::byte const> const onto{into.data(), received};  // received onto, exactly, this
          size_t const ip_header_size{sizeof(std::uint32_t) * (static_cast<std::uint8_t>(onto[0]) & 0x0F)};
          if (ip_header_size < IP_HEADER_SIZE_MIN || ip_header_size > IP_HEADER_SIZE_MAX) {
            ESP_LOGW(TAG, "received invalid IP header size (%zu bytes)", ip_header_size);
            continue;
          }
          std::span<std::byte const> const packet_span{onto.subspan(ip_header_size)};  // packet follows IP header
          if (!Packet::fits(packet_span)) {
            ESP_LOGW(TAG, "received invalid packet size (%zu bytes)", packet_span.size());
            continue;
          }
          Packet const packet{packet_span};
          if (!packet.is_reply()) {
            ESP_LOGW(TAG, "received packet is not a valid reply");
            continue;
          }
          std::size_t const index{packet.id()};
          if (index >= this->targets_.size()) {
            ESP_LOGW(TAG, "received packet with invalid id %zu", index);
            continue;
          }
          auto const timepoint{packet.timepoint()};
          this->targets_[index]->reply(endpoint, packet.sequence(), timepoint);
        }
        this->socket_->close(ec);
        if (ec) {
          ESP_LOGW(TAG, "abort: socket close error: %s", ec.message().c_str());
        } else {
          ESP_LOGD(TAG, "abort: socket closed");
        }
        co_return;
      },
      asio::detached);
}

bool Ping::teardown() {
  // undo setup
  for (auto &target : this->targets_) {
    if (target->timer_) {
      auto const count{target->timer_->cancel()};
      ESP_LOGD(TAG, "teardown: %s timer cancelled %zu operations", target->tag_.c_str(), count);
    }
  }
  if (this->socket_ && this->socket_->is_open()) {
    std::error_code ec;
    this->socket_->cancel(ec);
    if (ec) {
      ESP_LOGW(TAG, "teardown: socket cancel error: %s", ec.message().c_str());
    } else {
      ESP_LOGD(TAG, "teardown: socket cancelled");
    }
  }
  size_t sum{0};
  while (auto const addend{this->io_.poll()}) {
    sum += addend;
  }
  ESP_LOGD(TAG, "teardown: poll completed %zu operations", sum);
  if (this->socket_) {
    this->socket_.reset();
    ESP_LOGD(TAG, "teardown: socket reset");
  }
  return true;
}

void Ping::loop() { this->io_.poll_one(); }

void Ping::set_none(binary_sensor::BinarySensor *const none) {
  this->none_ = none;
  this->none_->publish_state(false);
}
void Ping::set_some(binary_sensor::BinarySensor *const some) {
  this->some_ = some;
  this->some_->publish_state(false);
}
void Ping::set_all(binary_sensor::BinarySensor *const all) {
  this->all_ = all;
  this->all_->publish_state(false);
}
void Ping::set_count(sensor::Sensor *const count) {
  this->count_ = count;
  this->count_->publish_state(0);
}
void Ping::set_since(since_::Since *const since) {
  this->since_ = since;
  this->since_->update();
}

void Ping::publish() {
  auto unpublished{false};  // until an enabled target is unpublished
  auto none{true};          // until an enabled target is successful
  auto all{true};           // until an enabled target is not successful
  size_t count{0};          // count of enabled successful targets
  asio::steady_timer::time_point latest{std::chrono::steady_clock::time_point::min()};  // latest on target
  for (auto const *const target : this->targets_) {
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
      if (latest < target->change_timepoint_) {
        latest = target->change_timepoint_;
      }
    }
  }
  if (all || unpublished)
    none = false;
  auto const some{!all && !none};

  if (this->none_)
    this->none_->publish_state(none);
  if (this->some_)
    this->some_->publish_state(some);
  if (this->all_)
    this->all_->publish_state(all);
  if (this->count_)
    this->count_->publish_state(static_cast<float>(count));
  if (this->since_)
    this->since_->set_when(latest);
}

}  // namespace ping_
}  // namespace esphome

#pragma GCC diagnostic pop