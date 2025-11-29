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

#include "format.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <format>

namespace esphome {
namespace format_ {

std::string duration(float const seconds) {
  if (std::isnan(seconds)) {
    return "NA";
  }

  auto const [d, h, m, s] = [](uint32_t remainder) -> std::array<uint32_t, 4> {
    constexpr uint32_t SPM{60};
    constexpr uint32_t SPH{60 * SPM};
    constexpr uint32_t SPD{24 * SPH};

    std::array<uint32_t, 4> result;
    auto it{result.begin()};

    *it++ = remainder / SPD;
    remainder %= SPD;

    *it++ = remainder / SPH;
    remainder %= SPH;

    *it++ = remainder / SPM;
    remainder %= SPM;

    *it++ = remainder;

    return result;
  }(static_cast<uint32_t>(seconds));

  return std::format("{} {:02d}:{:02d}:{:02d}", d, h, m, s);
}

}  // namespace format_
}  // namespace esphome

#pragma GCC diagnostic pop