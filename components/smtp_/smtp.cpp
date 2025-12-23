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

#include "smtp.hpp"

#include <chrono>
#include <format>
#include <ranges>
#include <thread>

// provide code generated from asio includes that follow below
// visibility to our ASIO_NO_EXCEPTIONS asio::detail::throw_exception definition,
// if called for, so that they can implicitly instantiate what they need.
#include "esphome/components/asio_/asio_detail_throw_exception_.cpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsuggest-override"
#pragma GCC diagnostic ignored "-Wc++11-compat"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#include <asio/buffer.hpp>
#include <asio/co_spawn.hpp>
#include <asio/connect.hpp>
#include <asio/detached.hpp>
#include <asio/read_until.hpp>
#include <asio/redirect_error.hpp>
#include <asio/ssl.hpp>
#include <asio/streambuf.hpp>
#pragma GCC diagnostic pop

#include "esphome/core/log.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include "esp_flash_encrypt.h"
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#include "mbedtls/net_sockets.h"
#include "mbedtls/error.h"
#include "mbedtls/base64.h"
#pragma GCC diagnostic pop

namespace esphome {
namespace smtp_ {

namespace {

#include "concat.hpp"

constexpr auto TAG{"smtp_"};

constexpr char const CRLF[]{"\r\n"};  // SMTP protocol line terminator

// wrap mbedtls function result value with methods to interpret success or error
class MbedTlsResult {
 private:
  int value_;

 public:
  MbedTlsResult() : value_{0} {};
  MbedTlsResult(int const value) : value_{value} {}
  operator int() const { return this->value_; }
  bool is_success() const { return 0 == this->value_; }
  bool is_error() const { return 0 > this->value_; }
  std::string to_string() const {
    if (0 > this->value_) {
      char strerror[128];
      mbedtls_strerror(this->value_, strerror, sizeof strerror);
      return {strerror};
    }
    return {};
  }
};

// return size needed to base64_encode a value
constexpr size_t base64_encoded_size(size_t const decoded_size) {
  constexpr size_t decoded{3};
  constexpr size_t encoded{4};
  return ((decoded_size + (decoded - 1)) / decoded) * encoded;
}

// base64_encode in to out with SMTP line terminator appended
MbedTlsResult base64_encode(std::string_view const in, std::string &out) {
  auto const encoded_size_in{base64_encoded_size(in.size()) + 1};
  auto encoded{std::make_unique<char[]>(encoded_size_in)};
  size_t encoded_size_out;
  MbedTlsResult const result{mbedtls_base64_encode(reinterpret_cast<unsigned char *>(encoded.get()), encoded_size_in,
                                                   &encoded_size_out,
                                                   reinterpret_cast<unsigned char const *>(in.data()), in.size())};
  if (!result.is_error()) {
    out = std::string(encoded.get()) + CRLF;
  }
  return result;
}

// wrap smtp Reply code and text with methods to interpret success or error
class Reply {
 private:
  std::error_code const ec_;
  int const code_;
  std::string const text_;

 public:
  explicit Reply(std::error_code const ec) : ec_{ec}, code_{}, text_{} {}
  explicit Reply(int const code, std::string_view const text = "") : ec_{}, code_{code}, text_{text} {}
  bool is_positive_completion() const { return !this->ec_ && 200 <= this->code_ && this->code_ < 300; }
  bool is_positive_intermediate() const { return !this->ec_ && 300 <= this->code_ && this->code_ < 400; }
  bool is_negative_transient_completion() const { return !this->ec_ && 400 <= this->code_ && this->code_ < 500; }
  bool is_negative_permanent_completion() const { return !this->ec_ && 500 <= this->code_ && this->code_ < 600; }
  char const *text() const { return this->ec_ ? this->ec_.message().c_str() : this->text_.c_str(); }
};

template<typename AsyncReadStream, typename DynamicBuffer>
asio::awaitable<Reply> receive_reply(AsyncReadStream &stream, DynamicBuffer &buffer) {
  std::string text{};
  while (true) {
    // return the length of the sequence ending with the first CRLF
    // when the buffer sequence's get area contains CRLF (immediately, if already)
    std::error_code ec;
    auto const length{
        co_await asio::async_read_until(stream, buffer, CRLF, asio::redirect_error(asio::use_awaitable, ec))};
    if (ec) {
      ESP_LOGW(TAG, "read until error: %s", ec.message().c_str());
      co_return Reply{ec};
    }

    // consume the line after extracting it
    auto const begin{asio::buffers_begin(buffer.data())};
    auto const end{begin + static_cast<std::ptrdiff_t>(length - (sizeof(CRLF) - 1))};
    std::string const line{begin, end};
    buffer.consume(length);
    ESP_LOGI(TAG, "< %s", line.c_str());

    // split the line into code and text, return when complete
    constexpr size_t size{3};
    constexpr auto isdigit{[](char const c) { return std::isdigit(static_cast<unsigned char>(c)); }};
    if (size < line.size() && std::ranges::all_of(line | std::views::take(size), isdigit)) {
      auto const code{std::stoi(line.substr(0, size))};
      if (!text.empty())
        text += '\n';
      text += line.substr(size + 1);
      switch (line[size]) {
        case ' ':
          co_return Reply{code, text};  // last line
        case '-':
          continue;  // another line
        default:
          co_return Reply{-1, std::format("bad code termination in reply line: {}", line)};
      }
    } else {
      co_return Reply{-1, std::format("no code in reply line: {}", line)};
    }
  }
}

template<typename AsyncStream, typename DynamicBuffer>
asio::awaitable<Reply> command(AsyncStream &stream, DynamicBuffer &buffer, std::string_view const request,
                               std::string_view log = {}) {
  if (!log.data())
    log = request;
  ESP_LOGI(TAG, "> %.*s", log.size(), log.data());
  std::error_code ec;
  auto const length{co_await asio::async_write(stream, asio::const_buffer{request.data(), request.size()},
                                               asio::redirect_error(asio::use_awaitable, ec))};
  if (ec) {
    ESP_LOGW(TAG, "write error: %s", ec.message().c_str());
    co_return Reply{ec};
  }
  co_return co_await receive_reply(stream, buffer);
}

// ^ delegate command from null terminated std::array<char, size>
template<typename AsyncStream, typename DynamicBuffer, std::size_t size>
asio::awaitable<Reply> command(AsyncStream &stream, DynamicBuffer &buffer, std::array<char, size> const &request,
                               std::string_view log = {}) {
  co_return co_await command(stream, buffer, std::string_view{request.data(), size - 1}, log);
}

template<typename AsyncStream, typename DynamicBuffer>
asio::awaitable<Reply> greeting_and_ehlo(AsyncStream &stream, DynamicBuffer &buffer) {
  {
    ESP_LOGD(TAG, "server greeting");
    auto const reply{co_await receive_reply(stream, buffer)};
    if (!reply.is_positive_completion())
      co_return reply;
  }
  {
    static constexpr auto request{concat::array("EHLO esphome", CRLF)};
    auto const reply{co_await command(stream, buffer, request)};
    co_return reply;
  }
}

template<typename AsyncStream, typename DynamicBuffer>
asio::awaitable<Reply> send(AsyncStream &stream, DynamicBuffer &buffer, std::string_view from, std::string_view subject,
                            std::string_view body, std::string_view to) {
  {
    std::string const request{std::format("MAIL FROM:<{}>{}", from, CRLF)};
    auto const reply{co_await command(stream, buffer, request)};
    if (!reply.is_positive_completion()) {
      ESP_LOGW(TAG, "command MAIL FROM: %s", reply.text());
      co_return reply;
    }
  }
  {
    std::string const request{std::format("RCPT TO:<{}>{}", to, CRLF)};
    auto const reply{co_await command(stream, buffer, request)};
    if (!reply.is_positive_completion()) {
      ESP_LOGW(TAG, "command RCPT TO: %s", reply.text());
      co_return reply;
    }
  }
  {
    static constexpr auto request{concat::array("DATA", CRLF)};
    auto const reply{co_await command(stream, buffer, request)};
    if (!reply.is_positive_intermediate()) {
      ESP_LOGW(TAG, "command DATA: %s", reply.text());
      co_return reply;
    }
  }
  {
    static constexpr auto format{concat::array("From: {}", CRLF, "To: {}", CRLF, "Subject: {}", CRLF, CRLF)};
    std::string const request{std::format(format.data(), from, to, subject)};
    std::error_code ec;
    auto const length{co_await asio::async_write(stream, asio::const_buffer{request.data(), request.size()},
                                                 asio::redirect_error(asio::use_awaitable, ec))};
    if (ec) {
      ESP_LOGW(TAG, "command DATA From, To, Subject: %s", ec.message().c_str());
      co_return Reply{-1};
    }
    ESP_LOGI(TAG, "> %s", request.c_str());
  }
  if (!body.empty()) {
    std::error_code ec;
    auto const length{co_await asio::async_write(stream, asio::const_buffer{body.data(), body.size()},
                                                 asio::redirect_error(asio::use_awaitable, ec))};
    if (ec) {
      ESP_LOGW(TAG, "command DATA body: %s", ec.message().c_str());
      co_return Reply{-1};
    }
    ESP_LOGI(TAG, "> %.*s", body.size(), body.data());
  }
  {
    static constexpr auto request{concat::array(CRLF, ".", CRLF)};
    auto const reply{co_await command(stream, buffer, request)};
    if (!reply.is_positive_completion()) {
      ESP_LOGW(TAG, "command DATA end: %s", reply.text());
    }
    co_return reply;
  }
}

template<typename Function, typename Result = std::invoke_result_t<Function>>
  requires std::is_trivially_copyable_v<Result>  // implies std::atomic<Result> will work
asio::awaitable<Result> async_on_thread(Function &&function) {
  auto executor = co_await asio::this_coro::executor;

  std::atomic<Result> result{};
  std::atomic<bool> done{false};

  std::thread([&result, &done, executor, function = std::forward<Function>(function)]() mutable {
    result = function();
    done = true;
    asio::post(executor, []() {});  // post noop to wake the executor
  }).detach();

  while (!done) {
    co_await asio::post(executor, asio::use_awaitable);  // suspend until executor wakes
  }

  co_return result;
}

}  // namespace

Component::Component()
    : server_{},
      port_{587},
      username_{},
      password_{},
      from_{},
      to_{},
      starttls_{true},
      cas_{},
      io_{},
      queue_{},
      queue_timer_{},
      interval_timer_{},
      ssl_{asio::ssl::context::tlsv12_client},
      stream_{} {}

void Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SMTP Client:");
  ESP_LOGCONFIG(TAG, "  server: %s", this->server_.c_str());
  ESP_LOGCONFIG(TAG, "  port: %d", this->port_);
#if 0
  ESP_LOGCONFIG(TAG, "  username: %s", this->username_.c_str());
  ESP_LOGCONFIG(TAG, "  password: %s", this->password_.c_str());
#endif
  ESP_LOGCONFIG(TAG, "  from: %s", this->from_.c_str());
  ESP_LOGCONFIG(TAG, "  to: %s", this->to_.c_str());
  ESP_LOGCONFIG(TAG, "  starttls: %s", this->starttls_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  task_name: %s", this->task_name_);
  ESP_LOGCONFIG(TAG, "  task_priority: %s", this->task_priority_);
}

float Component::get_setup_priority() const { return esphome::setup_priority::AFTER_CONNECTION; }

void Component::setup() {
  ESP_LOGD(TAG, "setup");

  if (!this->cas_.empty()) {
    ESP_LOGD(TAG, "parse CA certificates");
    std::error_code ec;
    // must include the null terminator
    this->ssl_.add_certificate_authority(asio::const_buffer{this->cas_.c_str(), this->cas_.size() + 1}, ec);
    if (ec) {
      ESP_LOGW(TAG, "parse CA certificates failed: %s", ec.message().c_str());
      this->mark_failed();
      return;
    }
  }

  // we must wait until AFTER_CONNECTION for timer construction
  this->queue_timer_.emplace(this->io_);
  this->interval_timer_.emplace(this->io_);

  asio::co_spawn(
      this->io_,
      [this]() -> asio::awaitable<void> {
        while (true) {
          std::error_code ec;

          // a message might already be in the queue because
          //  * it was queued before our AFTER_CONNECTION setup priority time
          //  * it was queued during our interval timer pause
          //  * it was left on the queue because of a session failure
          if (this->queue_.empty()) {
            this->queue_timer_->expires_at(asio::steady_timer::time_point::max());
            co_await this->queue_timer_->async_wait(asio::redirect_error(asio::use_awaitable, ec));
            if (ec) {
              if (ec == asio::error::operation_aborted) {
                // cancelled by enqueue (!queue_.empty) or teardown (queue_.empty)
                if (this->queue_.empty()) {
                  ESP_LOGD(TAG, "abort: queue timer %s", ec.message().c_str());
                  break;  // teardown
                }
              } else {
                ESP_LOGW(TAG, "queue timer error: %s", ec.message().c_str());
              }
              ec.clear();
            }
          }

          // session
          auto shutdown{false};
          do {
            this->stream_.emplace(co_await asio::this_coro::executor, this->ssl_);

            this->stream_->set_verify_mode(this->cas_.empty() ? asio::ssl::verify_none : asio::ssl::verify_peer, ec);
            if (ec) {
              ESP_LOGW(TAG, "ssl stream set verify mode error: %s", ec.message().c_str());
              break;
            }
            {
              MbedTlsResult const result{mbedtls_ssl_set_hostname(
                  reinterpret_cast<mbedtls_ssl_context *>(this->stream_->native_handle()), this->server_.c_str())};
              if (result.is_error()) {
                ESP_LOGW(TAG, "ssl set hostname error: %s:", result.to_string().c_str());
                break;
              }
            }

            {
              asio::ip::tcp::resolver resolver{co_await asio::this_coro::executor};
              auto const endpoints{co_await resolver.async_resolve(this->server_, std::to_string(this->port_),
                                                                   asio::redirect_error(asio::use_awaitable, ec))};
              if (ec) {
                ESP_LOGW(TAG, "resolve %s, port %u error: %s", this->server_.c_str(), this->port_,
                         ec.message().c_str());
                break;
              }
              co_await asio::async_connect(this->stream_->lowest_layer(), endpoints,
                                           asio::redirect_error(asio::use_awaitable, ec));
              if (ec) {
                ESP_LOGW(TAG, "connect server %s, port %u error: %s", this->server_.c_str(), this->port_,
                         ec.message().c_str());
                break;
              }
            }

            this->stream_->lowest_layer().non_blocking(true, ec);
            if (ec) {
              ESP_LOGW(TAG, "set non blocking error: %s", ec.message().c_str());
              break;
            }

            // greeting_and_ehlo and ssl handshake ordered per this->starttls_
            asio::streambuf buffer;
            if (this->starttls_) {
              {
                auto const reply{co_await greeting_and_ehlo(this->stream_->next_layer(), buffer)};
                if (!reply.is_positive_completion()) {
                  ESP_LOGW(TAG, "greeting and ehlo: %s", reply.text());
                  break;
                }
              }
              {
                static constexpr auto request{concat::array("STARTTLS", CRLF)};
                auto const reply{co_await command(this->stream_->next_layer(), buffer, request)};
                if (!reply.is_positive_completion()) {
                  ESP_LOGW(TAG, "request STARTTLS: %s", reply.text());
                  break;
                }
              }
            }
            {
#if 0
              // the espressif/asio port of async_handshake is not asynchronous
              // esphome will complain it takes too long (~500 > 30ms)
              co_await this->stream_->async_handshake(asio::ssl::stream_base::client, asio::redirect_error(asio::use_awaitable, ec));
#else
              // co_await the equivalent performed on another thread
              ec = co_await async_on_thread([this]() {
                std::error_code ec_;
                this->stream_->lowest_layer().non_blocking(false, ec_);
                if (!ec_) {
                  this->stream_->handshake(asio::ssl::stream_base::client, ec_);
                  this->stream_->lowest_layer().non_blocking(true);
                }
                return ec_;
              });
#endif
              if (ec) {
                ESP_LOGW(TAG, "handshake error: %s", ec.message().c_str());
                break;
              }
            }
            shutdown = true;
            if (!this->starttls_) {
              auto const reply{co_await greeting_and_ehlo(*this->stream_, buffer)};
              if (!reply.is_positive_completion()) {
                ESP_LOGW(TAG, "greeting and ehlo: %s", reply.text());
                break;
              }
            }

            // login
            {
              static constexpr auto request{concat::array("AUTH LOGIN", CRLF)};
              auto const reply{co_await command(*this->stream_, buffer, request)};
              if (!reply.is_positive_intermediate()) {
                ESP_LOGW(TAG, "command AUTH LOGIN %s", reply.text());
                break;
              }
            }
            {
              std::string request;
              auto const result{base64_encode(this->username_, request)};
              if (result.is_error()) {
                ESP_LOGW(TAG, "base64_encode: %s", result.to_string().c_str());
                break;
              }
              auto const reply{co_await command(*this->stream_, buffer, request)};
              if (!reply.is_positive_intermediate()) {
                ESP_LOGW(TAG, "command AUTH LOGIN username: %s", reply.text());
                break;
              }
            }
            {
              std::string request;
              auto const result{base64_encode(this->password_, request)};
              if (result.is_error()) {
                ESP_LOGW(TAG, "base64_encode: %s", result.to_string().c_str());
                break;
              }
              static constexpr auto log{"<redacted>"};
              auto const reply{co_await command(*this->stream_, buffer, request, log)};
              if (!reply.is_positive_completion()) {
                ESP_LOGW(TAG, "command AUTH LOGIN password: %s", reply.text());
                break;
              }
            }

            // send each message in the queue until it is empty
            while (!this->queue_.empty()) {
              const auto &message{this->queue_.front()};
              auto const reply{co_await send(*this->stream_, buffer, this->from_, message.subject, message.body,
                                             message.to.empty() ? this->to_ : message.to)};
              if (!reply.is_positive_completion()) {
                break;  // try again next session
              }
              this->queue_.pop_front();
            }

            // quit session
            {
              static constexpr auto request{concat::array("QUIT", CRLF)};
              auto const reply{co_await command(*this->stream_, buffer, request)};
              if (!reply.is_positive_completion()) {
                ESP_LOGW(TAG, "command QUIT: %s", reply.text());
                break;
              }
            }
          } while (false);

          // session/stream cleanup
          if (this->stream_) {
            if (shutdown) {
              co_await this->stream_->async_shutdown(asio::redirect_error(asio::use_awaitable, ec));
              if (ec) {
                ESP_LOGW(TAG, "shutdown ssl stream error: %s", ec.message().c_str());
              }
            }
            if (this->stream_->lowest_layer().is_open()) {
              this->stream_->lowest_layer().close(ec);
              if (ec) {
                ESP_LOGW(TAG, "close socket error: %s", ec.message().c_str());
              }
            }
            this->stream_.reset();
          }

          // pause for a minute
          this->interval_timer_->expires_after(std::chrono::minutes(1));
          co_await this->interval_timer_->async_wait(asio::redirect_error(asio::use_awaitable, ec));
          if (ec == asio::error::operation_aborted) {
            ESP_LOGD(TAG, "abort: interval timer %s", ec.message().c_str());
            break;  // teardown
          } else if (ec) {
            ESP_LOGW(TAG, "interval timer error: %s", ec.message().c_str());
          }
        }

        // teardown cleanup
        this->queue_timer_.reset();
        this->interval_timer_.reset();
        co_return;
      },
      asio::detached);
}

bool Component::teardown() {
  // undo setup
  if (this->queue_timer_) {
    auto const count{this->queue_timer_->cancel()};
    ESP_LOGD(TAG, "teardown: queue timer cancelled %zu operations", count);
  }
  if (this->interval_timer_) {
    auto const count{this->interval_timer_->cancel()};
    ESP_LOGD(TAG, "teardown: interval timer cancelled %zu operations", count);
  }
  if (this->stream_ && this->stream_->lowest_layer().is_open()) {
    this->stream_->lowest_layer().cancel();
    ESP_LOGD(TAG, "teardown: stream cancel");
  }
  size_t sum{0};
  while (auto const addend{this->io_.poll()}) {
    sum += addend;
  }
  ESP_LOGD(TAG, "teardown: poll completed %zu operations", sum);
  return true;
}

void Component::enqueue(std::string const &subject, std::string const &body, std::string const &to) {
  ESP_LOGD(TAG, "enqueue %s", subject.c_str());
  this->queue_.emplace_back(subject, body, to);
  this->queue_timer_->cancel();
}

void Component::loop() { this->io_.poll_one(); }

void Component::set_server(std::string const &value) { this->server_ = value; }
void Component::set_port(uint16_t const value) { this->port_ = value; }
void Component::set_username(std::string const &value) { this->username_ = value; }
void Component::set_password(std::string const &value) {
  this->password_ = value;
  if (!esp_flash_encryption_enabled()) {
    ESP_LOGW(TAG, "flash encryption disabled");
  }
}
void Component::set_from(std::string const &value) { this->from_ = value; }
void Component::set_to(std::string const &value) { this->to_ = value; }
void Component::set_starttls(bool const value) { this->starttls_ = value; }
void Component::set_cas(std::string const &value) { this->cas_ = value; }

}  // namespace smtp_
}  // namespace esphome

#pragma GCC diagnostic pop
