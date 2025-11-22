#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wall"
#pragma GCC diagnostic warning "-Wextra"
#pragma GCC diagnostic warning "-Wpedantic"
#pragma GCC diagnostic warning "-Wconversion"
#pragma GCC diagnostic warning "-Wsign-conversion"
#pragma GCC diagnostic warning "-Wold-style-cast"
#pragma GCC diagnostic warning "-Wshadow"
#pragma GCC diagnostic warning "-Wnull-dereference"
#pragma GCC diagnostic warning "-Wformat=2"
#pragma GCC diagnostic warning "-Wsuggest-override"
#pragma GCC diagnostic warning "-Wzero-as-null-pointer-constant"

#include "smtp.h"

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

constexpr char const TAG[]{"_smtp"};

constexpr char CRLF[]{"\r\n"};  // SMTP protocol line terminator

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
  MbedTlsResult(int value) : value_{value} {}
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
MbedTlsResult ssl_send(mbedtls_ssl_context *ssl, std::string_view request, std::string_view log = {}) {
  if (!request.empty()) {
    if (!log.data())
      log = request;
    ESP_LOGI(TAG, "> %.*s", log.size(), log.data());
    auto *next{reinterpret_cast<unsigned char const *>(request.data())};
    auto left{request.size()};
    while (0 < left) {
      MbedTlsResult result{mbedtls_ssl_write(ssl, next, left)};
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
  Recv recv_;
  std::array<char, N> buffer_;

 public:
  StreamBuffer(Recv recv) : recv_{std::move(recv)} {
    // set get buffer base, gptr, egptr for empty buffer
    this->setg(this->buffer_.data(), this->buffer_.data(), this->buffer_.data());
  }

 protected:
  int_type underflow() override {
    if (!(this->gptr() < this->egptr())) {
      MbedTlsResult result{this->recv_(this->buffer_.data(), this->buffer_.size())};
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
  StreamBuffer<N> buffer;

 public:
  InputStream(Recv recv) : std::basic_istream<char>{&buffer}, buffer{std::move(recv)} {}
  // non-copyable
  InputStream(const InputStream &) = delete;
  InputStream &operator=(const InputStream &) = delete;
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
  InputStream<128> stream{[this](char *buffer, size_t length) { return recv(buffer, length); }};
};

// NetTransport provides an un-encrypted network transport
class NetTransport : public Transport {
 private:
  mbedtls_net_context *const context_;

 public:
  NetTransport(mbedtls_net_context *context) : context_{context} {}
  MbedTlsResult send(std::string_view request, std::string_view log) override {
    if (!request.empty()) {
      if (!log.data())
        log = request;
      ESP_LOGI(TAG, "> %.*s", log.size(), log.data());
      MbedTlsResult result{
          mbedtls_net_send(this->context_, reinterpret_cast<unsigned char const *>(request.data()), request.size())};
      if (result.is_error()) {
        ESP_LOGW(TAG, "mbedtls_net_send: %s", result.to_string().c_str());
        return result;
      }
    }
    return {static_cast<int>(request.size())};
  }
  MbedTlsResult recv(char *buffer, size_t length) override {
    MbedTlsResult result{mbedtls_net_recv(this->context_, reinterpret_cast<unsigned char *>(buffer), length)};
    if (result.is_error()) {
      ESP_LOGW(TAG, "mbedtls_net_recv: %s", result.to_string().c_str());
    }
    return result;
  }
};

// SslTransport provides an encrypted network transport
class SslTransport : public Transport {
 private:
  mbedtls_ssl_context *const context_;

 public:
  SslTransport(mbedtls_ssl_context *context) : context_{context} {}
  MbedTlsResult send(std::string_view request, std::string_view log) override {
    return ssl_send(this->context_, request, log);
  }
  MbedTlsResult recv(char *buffer, size_t length) override {
    while (true) {
      MbedTlsResult result{mbedtls_ssl_read(this->context_, reinterpret_cast<unsigned char *>(buffer), length)};
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
MbedTlsResult ssl_handshake(mbedtls_ssl_context *ssl) {
  MbedTlsResult result;
  {
    ESP_LOGD(TAG, "ssl handshake");
    while (true) {
      result = MbedTlsResult{mbedtls_ssl_handshake(ssl)};
      if (result.is_success())
        break;
      if (!result.is_ssl_want()) {
        ESP_LOGW(TAG, "mbedtls_ssl_handshake: %s", result.to_string().c_str());
        break;
      }
    }
  }
  {
    auto verified{(mbedtls_ssl_get_verify_result(ssl))};
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
  if (auto ciphersuite{mbedtls_ssl_get_ciphersuite(ssl)}) {
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
  const auto encoded_size_in{base64_encoded_size(in.size()) + 1};
  auto encoded{std::make_unique<char[]>(encoded_size_in)};
  size_t encoded_size_out;
  MbedTlsResult result{mbedtls_base64_encode(reinterpret_cast<unsigned char *>(encoded.get()), encoded_size_in,
                                             &encoded_size_out, reinterpret_cast<unsigned char const *>(in.data()),
                                             in.size())};
  if (!result.is_error()) {
    out = std::string(encoded.get()) + CRLF;
  }
  return result;
}

// wrap SMTP reply code and text with methods to interpret success or error
class SmtpReply {
 public:
  int code;
  std::string text;

  SmtpReply() = default;
  SmtpReply(int code_) : code{code_} {}
  SmtpReply(int code_, std::string_view text_) : code{code_}, text{text_} {}
  SmtpReply(MbedTlsResult result) : code{result}, text{result.to_string()} {}

  bool is_positive_completion() const { return 200 <= code && code < 300; }
  bool is_positive_intermediate() const { return 300 <= code && code < 400; }
  bool is_negative_transient_completion() const { return 400 <= code && code < 500; }
  bool is_negative_permanent_completion() const { return 500 <= code && code < 600; }
};

// like std::getline but with an SMTP line terminator
std::istream &getline(std::istream &istream, std::string &line) {
  constexpr auto CR = CRLF[0];
  constexpr auto LF = CRLF[1];
  line.clear();
  std::string segment;
  for (;;) {
    if (!std::getline(istream, segment, LF))
      return istream;
    if (!segment.empty() && CR == segment.back()) {
      segment.pop_back();
      line += segment;
      return istream;
    }
    line += segment;
    line.push_back(LF);
  }
}

// perform an SMTP command by sending the request and compiling the reply over a transport
SmtpReply command(Transport &transport, std::string_view request, std::string_view log = {}) {
  MbedTlsResult result{transport.send(request, log)};
  if (result.is_error())
    return {result};

  std::string line;
  std::string text;
  while (getline(transport.stream, line)) {
    ESP_LOGI(TAG, "< %s", line.c_str());
    // parse line with a 3 digit code, character separator and text
    constexpr size_t size{3};
    constexpr auto isdigit{[](char c) { return std::isdigit(static_cast<unsigned char>(c)); }};
    if (size < line.size() && std::ranges::all_of(line | std::views::take(size), isdigit)) {
      int code{std::stoi(line.substr(0, size))};
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
template<std::size_t size> SmtpReply command(Transport &transport, const std::array<char, size> &request) {
  return command(transport, std::string_view{request.data(), size - 1});
}

// gather/evaluate the SMTP server greeting and initiate a session (EHLO)
SmtpReply greeting_and_ehlo(Transport &transport) {
  {
    ESP_LOGD(TAG, "server greeting");
    SmtpReply reply{command(transport, {nullptr, 0})};
    if (!reply.is_positive_completion())
      return reply;
  }
  {
    static constexpr auto request{concat::array("EHLO esphome", CRLF)};
    SmtpReply reply{command(transport, request)};
    return reply;
  }
}

}  // namespace

void Component::setup() {
  ESP_LOGCONFIG(TAG, "setup");

  {
    ESP_LOGD(TAG, "seed random number generator");
    MbedTlsResult result{mbedtls_ctr_drbg_seed(&this->ctr_drbg_, mbedtls_entropy_func, &this->entropy_, nullptr, 0)};
    if (result.is_error()) {
      ESP_LOGW(TAG, "mbedtls_ctr_drbg_seed: %s", result.to_string().c_str());
      this->mark_failed();
      return;
    }
  }

  {
    ESP_LOGD(TAG, "ssl config defaults");
    MbedTlsResult result{mbedtls_ssl_config_defaults(&this->ssl_config_, MBEDTLS_SSL_IS_CLIENT,
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
    MbedTlsResult result{mbedtls_x509_crt_parse(
        &this->x509_crt_, reinterpret_cast<const unsigned char *>(this->cas_.c_str()), this->cas_.size() + 1)};
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
  auto task_priority = std::min(this->task_priority_, static_cast<unsigned>(configMAX_PRIORITIES) - 1);
  if (this->task_priority_ != task_priority) {
    ESP_LOGW(TAG, "task_priority %u capped to maxiumum %u", this->task_priority_, task_priority);
  }
  BaseType_t result{xTaskCreate(Component::run_that_, this->task_name_.c_str(),
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

void Component::enqueue(const std::string &subject, const std::string &body, const std::string &to) {
  auto *message{new Message{subject, body, to.empty() ? this->to_ : to}};
  ESP_LOGD(TAG, "enqueue %s", message->subject.c_str());
  if (pdPASS_ != xQueueGenericSend(this->queue_, &message, 0, queueSEND_TO_BACK_)) {
    ESP_LOGW(TAG, "enqueue %s: queue full, message dropped", message->subject.c_str());
    delete message;
  }
}

void Component::run_that_(void *that) { static_cast<Component *>(that)->run_(); }

void Component::run_() {
  ESP_LOGD(TAG, "run");
  while (true) {
    Message *message;
    // block until there is a message to send
    if (pdTRUE_ == xQueuePeek(this->queue_, &message, portMAX_DELAY_)) {
      ESP_LOGD(TAG, "send message: %s", message->subject.c_str());

      // call send_ with a lambda
      // that it will use to dequeue and send each immediately available message
      // once a session context is established.
      // send_ will abort and return an error if there was a problem
      // establishing a session or sending a message.
      // as long as a message was not dequeued, we will try again.
      std::string context{"session"};
      auto error{this->send_([this, &context]() -> std::unique_ptr<Message> {
        // dequeue the next message without blocking
        // and transfer ownwership to caller
        Message *message_;
        if (pdTRUE_ == xQueueReceive(this->queue_, &message_, 0)) {
          ESP_LOGD(TAG, "dequeue %s", message_->subject.c_str());
          context = std::string("message: ") + message_->subject;
          return std::unique_ptr<Message>{message_};
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
        auto headroom{uxTaskGetStackHighWaterMark(nullptr)};
        ESP_LOGD(TAG, "stack headroom %u", static_cast<unsigned>(headroom));
      }

      // pause before trying again
      vTaskDelay(DELAY);
    }
  }
}

std::optional<std::string> Component::send_(std::function<std::unique_ptr<Message>()> dequeue) {
  // connect
  auto net{raii::make(mbedtls_net_init, mbedtls_net_free)};
  {
    ESP_LOGD(TAG, "net connect to %s:%d", this->server_.c_str(), this->port_);
    std::string port{std::format("{}", this->port_)};
    MbedTlsResult result{mbedtls_net_connect(&net, this->server_.c_str(), port.c_str(), MBEDTLS_NET_PROTO_TCP)};
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
    MbedTlsResult result{mbedtls_ssl_set_hostname(&ssl, this->server_.c_str())};
    if (result.is_error())
      return std::format("mbedtls_ssl_set_hostname: {}", result.to_string());
  }
  {
    ESP_LOGD(TAG, "ssl setup");
    MbedTlsResult result{mbedtls_ssl_setup(&ssl, &this->ssl_config_)};
    if (result.is_error())
      return std::format("mbedtls_ssl_setup: {}", result.to_string());
  }
  mbedtls_ssl_set_bio(&ssl, &net, mbedtls_net_send, mbedtls_net_recv, nullptr /* non-blocking I/O */);

  SslTransport transport{&ssl};

  // negotiate an encrypted session
  if (this->starttls_) {
    NetTransport net_transport{&net};
    {
      SmtpReply reply{greeting_and_ehlo(net_transport)};
      if (!reply.is_positive_completion())
        return std::format("greeting and ehlo: {}", reply.text);
    }
    {
      static constexpr auto request{concat::array("STARTTLS", CRLF)};
      SmtpReply reply{command(net_transport, request)};
      if (!reply.is_positive_completion())
        return std::format("command STARTTLS: {}", reply.text);
    }
  }
  {
    MbedTlsResult result{ssl_handshake(&ssl)};
    if (result.is_error())
      return std::format("ssl handshake: {}", result.to_string());
  }
  if (!this->starttls_) {
    SmtpReply reply{greeting_and_ehlo(transport)};
    if (!reply.is_positive_completion())
      return std::format("greeting and ehlo: {}", reply.text);
  }

  // login
  {
    static constexpr auto request{concat::array("AUTH LOGIN", CRLF)};
    SmtpReply reply{command(transport, request)};
    if (!reply.is_positive_intermediate())
      return std::format("command AUTH LOGIN: {}", reply.text);
  }
  {
    std::string request;
    MbedTlsResult result{base64_encode(this->username_, request)};
    if (result.is_error())
      return std::format("base64_encode: {}", result.to_string());
    SmtpReply reply{command(transport, request)};
    if (!reply.is_positive_intermediate())
      return std::format("command AUTH LOGIN username: {}", reply.text);
  }
  {
    std::string request;
    MbedTlsResult result{base64_encode(this->password_, request)};
    if (result.is_error())
      return std::format("base64_encode: {}", result.to_string());
    static constexpr char log[]{"<redacted>"};
    SmtpReply reply{command(transport, request, log)};
    if (!reply.is_positive_completion())
      return std::format("command AUTH LOGIN password: {}", reply.text);
  }

  // send each message in the queue
  while (auto message{dequeue()}) {
    {
      std::string request{std::format("MAIL FROM:<{}>{}", this->from_, CRLF)};
      SmtpReply reply{command(transport, request)};
      if (!reply.is_positive_completion())
        return std::format("command MAIL FROM: {}", reply.text);
    }
    {
      std::string request{std::format("RCPT TO:<{}>{}", message->to, CRLF)};
      SmtpReply reply{command(transport, request)};
      if (!reply.is_positive_completion())
        return std::format("command RCPT TO: {}", reply.text);
    }
    {
      static constexpr auto request{concat::array("DATA", CRLF)};
      SmtpReply reply{command(transport, request)};
      if (!reply.is_positive_intermediate())
        return std::format("command DATA: {}", reply.text);
    }
    {
      static constexpr auto format{concat::array("From: {}", CRLF, "To: {}", CRLF, "Subject: {}", CRLF, CRLF)};
      std::string request{std::format(format.data(), this->from_, message->to, message->subject)};
      MbedTlsResult result{ssl_send(&ssl, request)};
      if (result.is_error())
        return std::format("command DATA From, To, Subject: {}", result.to_string());
    }
    {
      MbedTlsResult result{ssl_send(&ssl, message->body)};
      if (result.is_error())
        return std::format("command DATA body: {}", result.to_string());
    }
    {
      static constexpr auto request{concat::array(CRLF, ".", CRLF)};
      SmtpReply reply{command(transport, request)};
      if (!reply.is_positive_completion())
        return std::format("command DATA end: {}", reply.text);
    }
  }

  // end session
  {
    static constexpr auto request{concat::array("QUIT", CRLF)};
    SmtpReply reply{command(transport, request)};
    if (!reply.is_positive_completion())
      return std::format("command QUIT: {}", reply.text);
  }

  // success
  return {};
}

}  // namespace smtp_
}  // namespace esphome

#pragma GCC diagnostic pop
