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

#include "format.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <format>

namespace esphome {
namespace _format {

std::string duration(float seconds) {
  if (std::isnan(seconds)) {
    return "NA";
  }

  auto [d, h, m, s] = [](uint32_t remainder) -> std::array<uint32_t, 4> {
    static constexpr uint32_t SPM{60};
    static constexpr uint32_t SPH{60 * SPM};
    static constexpr uint32_t SPD{24 * SPH};

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

}  // namespace _format
}  // namespace esphome

#pragma GCC diagnostic pop
