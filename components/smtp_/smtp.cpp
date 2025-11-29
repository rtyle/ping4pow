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

#include <format>
#include <iostream>
#include <ranges>

#include "esphome/core/log.h"

#include "mbedtls/net_sockets.h"
#include "mbedtls/error.h"
#include "mbedtls/base64.h"

namespace esphome {
namespace smtp_ {

namespace {

#include "concat.hpp"

constexpr auto TAG{"_smtp"};

constexpr char const CRLF[]{"\r\n"};  // SMTP protocol line terminator

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
inline constexpr auto DELAY{pdMS_TO_TICKS(60'000)};
inline constexpr auto pdPASS_{pdPASS};
inline constexpr auto pdTRUE_{pdTRUE};
inline constexpr auto portMAX_DELAY_{portMAX_DELAY};
inline constexpr auto queueQUEUE_TYPE_BASE_{queueQUEUE_TYPE_BASE};
inline constexpr auto queueSEND_TO_BACK_{queueSEND_TO_BACK};
#pragma GCC diagnostic pop

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
  bool is_ssl_want_read() const { return MBEDTLS_ERR_SSL_WANT_READ == this->value_; }
  bool is_ssl_want_write() const { return MBEDTLS_ERR_SSL_WANT_WRITE == this->value_; }
  bool is_ssl_want() const { return is_ssl_want_read() || is_ssl_want_write(); }
  std::string to_string() const {
    if (0 > this->value_) {
      char strerror[128];
      mbedtls_strerror(this->value_, strerror, sizeof strerror);
      return {strerror};
    }
    return {};
  }
};

// send an encrypted request
MbedTlsResult ssl_send(mbedtls_ssl_context &ssl, std::string_view const request, std::string_view log = {}) {
  if (!request.empty()) {
    if (!log.data())
      log = request;
    ESP_LOGI(TAG, "> %.*s", log.size(), log.data());
    auto *next{reinterpret_cast<unsigned char const *>(request.data())};
    auto left{request.size()};
    while (0 < left) {
      MbedTlsResult const result{mbedtls_ssl_write(&ssl, next, left)};
      if (result.is_error()) {
        if (result.is_ssl_want())
          continue;
        ESP_LOGW(TAG, "mbedtls_ssl_write: %s", result.to_string().c_str());
        return result;
      }
      left -= static_cast<size_t>(result);
      next += static_cast<size_t>(result);
    }
  }
  return {static_cast<int>(request.size())};
}

// signature of a Recv function needed to construct a StreamBuffer
using Recv = std::function<MbedTlsResult(char *, size_t)>;

// StreamBuffer buffers Recv'd data for line-oriented input
template<size_t N> class StreamBuffer : public std::basic_streambuf<char> {
 private:
  using int_type = typename std::char_traits<char>::int_type;
  Recv const recv_;
  std::array<char, N> buffer_;

 public:
  StreamBuffer(Recv recv) : recv_{std::move(recv)} {
    // set get buffer base, gptr, egptr for empty buffer
    this->setg(this->buffer_.data(), this->buffer_.data(), this->buffer_.data());
  }

 protected:
  int_type underflow() override {
    if (!(this->gptr() < this->egptr())) {
      auto const result{this->recv_(this->buffer_.data(), this->buffer_.size())};
      if (result.is_error() || 0 == result)
        return std::char_traits<char>::eof();
      this->setg(this->buffer_.data(), this->buffer_.data(), this->buffer_.data() + static_cast<int>(result));
    }
    return std::char_traits<char>::to_int_type(*this->gptr());
  }
};

// InputStream uses StreamBuffer for Recv'd line-oriented input
template<size_t N> class InputStream : public std::basic_istream<char> {
 private:
  StreamBuffer<N> stream_buffer_;

 public:
  InputStream(Recv const recv) : std::basic_istream<char>{&stream_buffer_}, stream_buffer_{std::move(recv)} {}
  // non-copyable
  InputStream(InputStream const &) = delete;
  InputStream &operator=(InputStream const &) = delete;
  // moveable
  InputStream(InputStream &&) = default;
  InputStream &operator=(InputStream &&) = default;
};

// Transport is an abstract base class
// whose derivations need to support send and recv
class Transport {
 public:
  virtual ~Transport() = default;
  virtual MbedTlsResult send(std::string_view, std::string_view) = 0;
  virtual MbedTlsResult recv(char *, size_t) = 0;
  InputStream<128> stream{[this](char *const buffer, size_t const length) { return recv(buffer, length); }};
};

// NetTransport provides an un-encrypted network transport
class NetTransport : public Transport {
 private:
  mbedtls_net_context &context_;

 public:
  NetTransport(mbedtls_net_context &context) : context_{context} {}
  MbedTlsResult send(std::string_view const request, std::string_view log) override {
    if (!request.empty()) {
      if (!log.data())
        log = request;
      ESP_LOGI(TAG, "> %.*s", log.size(), log.data());
      MbedTlsResult const result{
          mbedtls_net_send(&this->context_, reinterpret_cast<unsigned char const *>(request.data()), request.size())};
      if (result.is_error()) {
        ESP_LOGW(TAG, "mbedtls_net_send: %s", result.to_string().c_str());
        return result;
      }
    }
    return {static_cast<int>(request.size())};
  }
  MbedTlsResult recv(char *const buffer, size_t const length) override {
    MbedTlsResult const result{mbedtls_net_recv(&this->context_, reinterpret_cast<unsigned char *>(buffer), length)};
    if (result.is_error()) {
      ESP_LOGW(TAG, "mbedtls_net_recv: %s", result.to_string().c_str());
    }
    return result;
  }
};

// SslTransport provides an encrypted network transport
class SslTransport : public Transport {
 private:
  mbedtls_ssl_context &context_;

 public:
  SslTransport(mbedtls_ssl_context &context) : context_{context} {}
  MbedTlsResult send(std::string_view const request, std::string_view const log) override {
    return ssl_send(this->context_, request, log);
  }
  MbedTlsResult recv(char *const buffer, size_t const length) override {
    while (true) {
      MbedTlsResult const result{mbedtls_ssl_read(&this->context_, reinterpret_cast<unsigned char *>(buffer), length)};
      if (result.is_error()) {
        if (result.is_ssl_want())
          continue;
        ESP_LOGW(TAG, "mbedtls_ssl_read: %s", result.to_string().c_str());
        return result;
      }
      return result;
    }
  }
};

// perform an ssl handshake with certificate validation
MbedTlsResult ssl_handshake(mbedtls_ssl_context &ssl) {
  MbedTlsResult result;
  {
    ESP_LOGD(TAG, "ssl handshake");
    while (true) {
      result = MbedTlsResult{mbedtls_ssl_handshake(&ssl)};
      if (result.is_success())
        break;
      if (!result.is_ssl_want()) {
        ESP_LOGW(TAG, "mbedtls_ssl_handshake: %s", result.to_string().c_str());
        break;
      }
    }
  }
  {
    auto const verified{(mbedtls_ssl_get_verify_result(&ssl))};
    switch (static_cast<int>(verified)) {
      case 0:
        ESP_LOGD(TAG, "ssl certificate verified");
        break;
      case -1:
        ESP_LOGW(TAG, "ssl certificate verify info not available");
        break;
      default: {
        char info[64];
        info[mbedtls_x509_crt_verify_info(info, sizeof info - 1, "", verified)] = 0;
        ESP_LOGD(TAG, "ssl cerfificate verify info: %s", info);
      }
    }
  }
  if (auto const ciphersuite{mbedtls_ssl_get_ciphersuite(&ssl)}) {
    ESP_LOGD(TAG, "ssl cipher suite: %s", ciphersuite);
  }
  return result;
}

// return size needed to base64_encode a value
constexpr size_t base64_encoded_size(size_t decoded_size) {
  constexpr size_t decoded{3};
  constexpr size_t encoded{4};
  return ((decoded_size + (decoded - 1)) / decoded) * encoded;
}

// base64_encode in to out with SMTP line terminator appended
MbedTlsResult base64_encode(std::string_view in, std::string &out) {
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

// wrap SMTP reply code and text with methods to interpret success or error
class SmtpReply {
 private:
  int const code_;
  std::string text_;

 public:
  SmtpReply(int const code) : code_{code} {}
  SmtpReply(int const code, std::string_view const text) : code_{code}, text_{text} {}
  SmtpReply(MbedTlsResult const result) : code_{result}, text_{result.to_string()} {}

  int get_code() const { return this->code_; }
  std::string_view get_text() const { return this->text_; }

  bool is_positive_completion() const { return 200 <= this->code_ && this->code_ < 300; }
  bool is_positive_intermediate() const { return 300 <= this->code_ && this->code_ < 400; }
  bool is_negative_transient_completion() const { return 400 <= this->code_ && this->code_ < 500; }
  bool is_negative_permanent_completion() const { return 500 <= this->code_ && this->code_ < 600; }
};

// like std::getline but with an SMTP line terminator
std::istream &getline(std::istream &istream, std::string &line) {
  constexpr auto cr = CRLF[0];
  constexpr auto lf = CRLF[1];
  line.clear();
  std::string segment;
  for (;;) {
    if (!std::getline(istream, segment, lf))
      return istream;
    if (!segment.empty() && cr == segment.back()) {
      segment.pop_back();
      line += segment;
      return istream;
    }
    line += segment;
    line.push_back(lf);
  }
}

// perform an SMTP command by sending the request and compiling the reply over a transport
SmtpReply command(Transport &transport, std::string_view const request, std::string_view const log = {}) {
  MbedTlsResult const result{transport.send(request, log)};
  if (result.is_error())
    return {result};

  std::string line;
  std::string text;
  while (getline(transport.stream, line)) {
    ESP_LOGI(TAG, "< %s", line.c_str());
    // parse line with a 3 digit code, character separator and text
    constexpr size_t size{3};
    constexpr auto isdigit{[](char const c) { return std::isdigit(static_cast<unsigned char>(c)); }};
    if (size < line.size() && std::ranges::all_of(line | std::views::take(size), isdigit)) {
      int const code{std::stoi(line.substr(0, size))};
      if (!text.empty())
        text += '\n';
      text += line.substr(size + 1);
      switch (line[size]) {
        case ' ':
          return {code, text};  // last line
        case '-':
          continue;  // another line
        default:
          transport.stream.setstate(std::ios_base::failbit);
          return {MBEDTLS_ERR_ERROR_GENERIC_ERROR, std::format("bad code termination in reply line: {}", line)};
      }
    } else {
      transport.stream.setstate(std::ios_base::failbit);
      return {MBEDTLS_ERR_ERROR_GENERIC_ERROR, std::format("no code in reply line: {}", line)};
    }
  }
  transport.stream.setstate(std::ios_base::failbit);
  return {MBEDTLS_ERR_ERROR_GENERIC_ERROR, std::format("incomplete reply: {}", text)};
}

// ^ delegate command from null terminated std::array<char, size>
template<std::size_t size> SmtpReply command(Transport &transport, std::array<char, size> const &request) {
  return command(transport, std::string_view{request.data(), size - 1});
}

// gather/evaluate the SMTP server greeting and initiate a session (EHLO)
SmtpReply greeting_and_ehlo(Transport &transport) {
  {
    ESP_LOGD(TAG, "server greeting");
    auto const reply{command(transport, {nullptr, 0})};
    if (!reply.is_positive_completion())
      return reply;
  }
  {
    static constexpr auto request{concat::array("EHLO esphome", CRLF)};
    auto const reply{command(transport, request)};
    return reply;
  }
}

}  // namespace

void Component::setup() {
  ESP_LOGCONFIG(TAG, "setup");

  {
    ESP_LOGD(TAG, "seed random number generator");
    MbedTlsResult const result{
        mbedtls_ctr_drbg_seed(&this->ctr_drbg_, mbedtls_entropy_func, &this->entropy_, nullptr, 0)};
    if (result.is_error()) {
      ESP_LOGW(TAG, "mbedtls_ctr_drbg_seed: %s", result.to_string().c_str());
      this->mark_failed();
      return;
    }
  }

  {
    ESP_LOGD(TAG, "ssl config defaults");
    MbedTlsResult const result{mbedtls_ssl_config_defaults(&this->ssl_config_, MBEDTLS_SSL_IS_CLIENT,
                                                           MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)};
    if (result.is_error()) {
      ESP_LOGW(TAG, "mbedtls_ssl_config_defaults: %s", result.to_string().c_str());
      this->mark_failed();
      return;
    }
  }

  mbedtls_ssl_conf_rng(&this->ssl_config_, mbedtls_ctr_drbg_random, &this->ctr_drbg_);

  if (this->cas_.empty()) {
    mbedtls_ssl_conf_authmode(&this->ssl_config_, MBEDTLS_SSL_VERIFY_NONE);
  } else {
    ESP_LOGD(TAG, "parse CA certificates");
    MbedTlsResult const result{mbedtls_x509_crt_parse(
        &this->x509_crt_, reinterpret_cast<unsigned char const *>(this->cas_.c_str()), this->cas_.size() + 1)};
    if (result.is_error()) {
      ESP_LOGW(TAG, "mbedtls_x509_crt_parse: %s", result.to_string().c_str());
      this->mark_failed();
      return;
    }
    mbedtls_ssl_conf_authmode(&this->ssl_config_, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&this->ssl_config_, &this->x509_crt_, nullptr);
  }

  ESP_LOGD(TAG, "create queue");
  this->queue_ = xQueueGenericCreate(8, sizeof(Message *), queueQUEUE_TYPE_BASE_);
  if (!this->queue_) {
    ESP_LOGW(TAG, "xQueueCreate failed");
    this->mark_failed();
    return;
  }

  ESP_LOGD(TAG, "create task");
  auto const task_priority{std::min(this->task_priority_, static_cast<unsigned>(configMAX_PRIORITIES) - 1)};
  if (this->task_priority_ != task_priority) {
    ESP_LOGW(TAG, "task_priority %u capped to maxiumum %u", this->task_priority_, task_priority);
  }
  BaseType_t const result{xTaskCreate([](void *that) { static_cast<Component *>(that)->run_(); },
                                      this->task_name_.c_str(),
                                      8192,  // stack size tuned from logged headroom reports during run_
                                      this, task_priority, &this->task_handle_)};

  if (result != pdPASS_ || this->task_handle_ == nullptr) {
    ESP_LOGW(TAG, "xTaskCreate: %d", result);
    this->mark_failed();
    return;
  }
}

void Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SMTP Client:");
  ESP_LOGCONFIG(TAG, "  server: %s", this->server_.c_str());
  ESP_LOGCONFIG(TAG, "  port: %d", this->port_);
  ESP_LOGCONFIG(TAG, "  from: %s", this->from_.c_str());
  ESP_LOGCONFIG(TAG, "  to: %s", this->to_.c_str());
  ESP_LOGCONFIG(TAG, "  starttls: %s", this->starttls_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  task_name: %s", this->task_name_);
  ESP_LOGCONFIG(TAG, "  task_priority: %s", this->task_priority_);
}

void Component::enqueue(std::string const &subject, std::string const &body, std::string const &to) {
  auto const *message{new Message{subject, body, to.empty() ? this->to_ : to}};
  ESP_LOGD(TAG, "enqueue %s", message->subject.c_str());
  if (pdPASS_ != xQueueGenericSend(this->queue_, &message, 0, queueSEND_TO_BACK_)) {
    ESP_LOGW(TAG, "enqueue %s: queue full, message dropped", message->subject.c_str());
    delete message;
  }
}

void Component::run_() {
  ESP_LOGD(TAG, "run");
  while (true) {
    Message *peek;
    // block until there is a message to send
    if (pdTRUE_ == xQueuePeek(this->queue_, &peek, portMAX_DELAY_)) {
      ESP_LOGD(TAG, "send message: %s", message->subject.c_str());

      // call send_ with a lambda
      // that it will use to dequeue and send each immediately available message
      // once a session context is established.
      // send_ will abort and return an error if there was a problem
      // establishing a session or sending a message.
      // as long as a message was not dequeued, we will try again.
      std::string context{"session"};
      auto const error{this->send_([this, &context]() -> std::unique_ptr<Message> {
        // dequeue the next message without blocking
        // and transfer ownwership to caller
        Message *message;
        if (pdTRUE_ == xQueueReceive(this->queue_, &message, 0)) {
          ESP_LOGD(TAG, "dequeue %s", message->subject.c_str());
          context = std::string("message: ") + message->subject;
          return std::unique_ptr<Message>{message};
        }
        // queue is empty
        return nullptr;
      })};

      if (error.has_value()) {
        // send_ aborted with an error message. log it in context
        ESP_LOGW(TAG, "send %s: %s", context.c_str(), error->c_str());
      }

      {
        // used for tuning our task's stack size
        auto const headroom{uxTaskGetStackHighWaterMark(nullptr)};
        ESP_LOGD(TAG, "stack headroom %u", static_cast<unsigned>(headroom));
      }

      // pause before trying again
      vTaskDelay(DELAY);
    }
  }
}

std::optional<std::string> Component::send_(std::function<std::unique_ptr<Message>()> const dequeue) {
  // connect
  auto net{raii::make(mbedtls_net_init, mbedtls_net_free)};
  {
    ESP_LOGD(TAG, "net connect to %s:%d", this->server_.c_str(), this->port_);
    std::string const port{std::format("{}", this->port_)};
    MbedTlsResult const result{mbedtls_net_connect(&net, this->server_.c_str(), port.c_str(), MBEDTLS_NET_PROTO_TCP)};
    if (result.is_error())
      return std::format("mbedtls_net_connect: {}", result.to_string());
  }

  // ssl configuration
  auto ssl{raii::make(mbedtls_ssl_init, [](mbedtls_ssl_context *ssl_) {
    mbedtls_ssl_close_notify(ssl_);
    mbedtls_ssl_free(ssl_);
  })};
  {
    ESP_LOGD(TAG, "set hostname: %s", this->server_.c_str());
    MbedTlsResult const result{mbedtls_ssl_set_hostname(&ssl, this->server_.c_str())};
    if (result.is_error())
      return std::format("mbedtls_ssl_set_hostname: {}", result.to_string());
  }
  {
    ESP_LOGD(TAG, "ssl setup");
    MbedTlsResult const result{mbedtls_ssl_setup(&ssl, &this->ssl_config_)};
    if (result.is_error())
      return std::format("mbedtls_ssl_setup: {}", result.to_string());
  }
  mbedtls_ssl_set_bio(&ssl, &net, mbedtls_net_send, mbedtls_net_recv, nullptr /* non-blocking I/O */);

  SslTransport transport{ssl};

  // negotiate an encrypted session
  if (this->starttls_) {
    NetTransport net_transport{net};
    {
      auto const reply{greeting_and_ehlo(net_transport)};
      if (!reply.is_positive_completion())
        return std::format("greeting and ehlo: {}", reply.get_text());
    }
    {
      static constexpr auto request{concat::array("STARTTLS", CRLF)};
      auto const reply{command(net_transport, request)};
      if (!reply.is_positive_completion())
        return std::format("command STARTTLS: {}", reply.get_text());
    }
  }
  {
    MbedTlsResult const result{ssl_handshake(ssl)};
    if (result.is_error())
      return std::format("ssl handshake: {}", result.to_string());
  }
  if (!this->starttls_) {
    auto const reply{greeting_and_ehlo(transport)};
    if (!reply.is_positive_completion())
      return std::format("greeting and ehlo: {}", reply.get_text());
  }

  // login
  {
    static constexpr auto request{concat::array("AUTH LOGIN", CRLF)};
    auto const reply{command(transport, request)};
    if (!reply.is_positive_intermediate())
      return std::format("command AUTH LOGIN: {}", reply.get_text());
  }
  {
    std::string request;
    MbedTlsResult const result{base64_encode(this->username_, request)};
    if (result.is_error())
      return std::format("base64_encode: {}", result.to_string());
    auto const reply{command(transport, request)};
    if (!reply.is_positive_intermediate())
      return std::format("command AUTH LOGIN username: {}", reply.get_text());
  }
  {
    std::string request;
    MbedTlsResult const result{base64_encode(this->password_, request)};
    if (result.is_error())
      return std::format("base64_encode: {}", result.to_string());
    static constexpr auto log{"<redacted>"};
    auto const reply{command(transport, request, log)};
    if (!reply.is_positive_completion())
      return std::format("command AUTH LOGIN password: {}", reply.get_text());
  }

  // send each message in the queue
  while (auto message{dequeue()}) {
    {
      std::string const request{std::format("MAIL FROM:<{}>{}", this->from_, CRLF)};
      auto const reply{command(transport, request)};
      if (!reply.is_positive_completion())
        return std::format("command MAIL FROM: {}", reply.get_text());
    }
    {
      std::string const request{std::format("RCPT TO:<{}>{}", message->to, CRLF)};
      auto const reply{command(transport, request)};
      if (!reply.is_positive_completion())
        return std::format("command RCPT TO: {}", reply.get_text());
    }
    {
      static constexpr auto request{concat::array("DATA", CRLF)};
      auto const reply{command(transport, request)};
      if (!reply.is_positive_intermediate())
        return std::format("command DATA: {}", reply.get_text());
    }
    {
      static constexpr auto format{concat::array("From: {}", CRLF, "To: {}", CRLF, "Subject: {}", CRLF, CRLF)};
      std::string const request{std::format(format.data(), this->from_, message->to, message->subject)};
      MbedTlsResult const result{ssl_send(ssl, request)};
      if (result.is_error())
        return std::format("command DATA From, To, Subject: {}", result.to_string());
    }
    {
      MbedTlsResult const result{ssl_send(ssl, message->body)};
      if (result.is_error())
        return std::format("command DATA body: {}", result.to_string());
    }
    {
      static constexpr auto request{concat::array(CRLF, ".", CRLF)};
      auto const reply{command(transport, request)};
      if (!reply.is_positive_completion())
        return std::format("command DATA end: {}", reply.get_text());
    }
  }

  // end session
  {
    static constexpr auto request{concat::array("QUIT", CRLF)};
    auto const reply{command(transport, request)};
    if (!reply.is_positive_completion())
      return std::format("command QUIT: {}", reply.get_text());
  }

  // success
  return {};
}

}  // namespace smtp_
}  // namespace esphome

#pragma GCC diagnostic pop
