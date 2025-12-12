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

#include <system_error>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-override"
#include <asio/execution_context.hpp>
#include <asio/ip/bad_address_cast.hpp>
#pragma GCC diagnostic pop

#include "esphome/components/asio_/asio_detail_throw_exception_.cpp"

namespace asio {
namespace detail {

#if defined(ASIO_NO_EXCEPTIONS)

// asio library source compiled with ASIO_NO_EXCEPTIONS
// never implicitly instantiate (Weakly define) a throw_exception template
// because <asio/detail/throw_exception.hpp> does not define the template to do so.
// instead, the compiled code has an Undefined reference to such that the linker must resolve.
// asio_throw_exception.hpp has provided the template.
// we provide the definitions that we have seen the linker complain about here.
template void throw_exception<>(std::system_error const &e ASIO_SOURCE_LOCATION_DEFAULTED_PARAM);
template void throw_exception<>(asio::invalid_service_owner const &e ASIO_SOURCE_LOCATION_DEFAULTED_PARAM);
template void throw_exception<>(asio::service_already_exists const &e ASIO_SOURCE_LOCATION_DEFAULTED_PARAM);
template void throw_exception<>(asio::ip::bad_address_cast const &e ASIO_SOURCE_LOCATION_DEFAULTED_PARAM);

#endif  // !defined(ASIO_NO_EXCEPTIONS)

}  // namespace detail
}  // namespace asio

#pragma GCC diagnostic pop
