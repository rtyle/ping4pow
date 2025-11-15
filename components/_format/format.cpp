#include "format.h"

#include <cmath>
#include <cstdint>

#include <array>
#include <sstream>
#include <iomanip>

namespace esphome {
namespace _format {

std::string duration(float seconds) {
  if (std::isnan(seconds)) {
    return "NA";
  }

  auto result{[](uint32_t remainder) -> std::array<uint32_t, 4> {
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
  }(static_cast<uint32_t>(seconds))};
  auto it{result.begin()};

  return (std::stringstream{}
    << *it << " "
    << std::setfill('0')
    << std::setw(2) << *(++it) << ":"
    << std::setw(2) << *(++it) << ":"
    << std::setw(2) << *(++it)
  ).str();
}

}  // namespace _format
}  // namespace esphome
