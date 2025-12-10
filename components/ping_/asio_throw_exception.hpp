#pragma once

#if defined(ASIO_NO_EXCEPTIONS)

#include <asio/detail/config.hpp>           // ASIO_NO_EXCEPTIONS
#include <asio/detail/throw_exception.hpp>  // throw_exception if !defined(ASIO_NO_EXCEPTIONS)

#include "esphome/core/log.h"
#include "esp_system.h"

namespace asio {
namespace detail {

inline constexpr auto TAG{"asio"};

template<typename Exception> [[noreturn]] inline void throw_exception(const Exception &e ASIO_SOURCE_LOCATION_PARAM) {
  ESP_LOGE(TAG, "ASIO_NO_EXCEPTIONS abort: %s", e.what());
  esp_system_abort("ASIO_NO_EXCEPTIONS");
}

}  // namespace detail
}  // namespace asio

#endif  // ASIO_NO_EXCEPTIONS