#pragma once

#include <array>
#include <string_view>
#include <stdexcept>

namespace concat {

namespace detail {

// return size of null terminated string_view input
constexpr std::size_t size(std::string_view input) {
  auto size_{input.size()};
#if defined(__cpp_exceptions)
  if (!size_)
    throw std::invalid_argument("input must be null terminated");
#endif
  return size_ - 1;
}

// ^ delegate size from char(&)[size] or std::array<char, size>
template<std::size_t size_> constexpr std::size_t size(const char (&input)[size_]) {
  return size(std::string_view{input, size_});
}
template<std::size_t size_> constexpr std::size_t size(const std::array<char, size_> &input) {
  return size(std::string_view{input.data(), size_});
}

// copy null terminated input to next part in output
constexpr void copy(char *&output, std::string_view input) {
  char last{1};  // not null
  for (char next : input)
    *output++ = last = next;  // copy including last (terminator)
#if defined(__cpp_exceptions)
  if (last)
    throw std::invalid_argument("input must be null terminated");
#endif
  --output;  // back up over null terminator in preparation for next input
}

// ^ delegate copy from char(&)[size] or std::array<char, size>
template<std::size_t size> constexpr void copy(char *&output, const char (&input)[size]) {
  copy(output, std::string_view(input, size));
}
template<std::size_t size> constexpr void copy(char *&output, const std::array<char, size> &input) {
  copy(output, std::string_view(input.data(), size));
}

}  // namespace detail

// return a compile-time std::array concatenation of all inputs
// (null terminated char(&)[size] or std::array<char, size>)
template<typename... Inputs> consteval auto array(const Inputs &...inputs) {
  // output size is the sum of each null terminated input string plus 1
  std::array<char, (detail::size(inputs) + ...) + 1> output;
  // for each of the inputs, copy to the next part in output
  char *next{output.data()};
  (..., detail::copy(next, inputs));
  return output;
}

// return a std::string_view of a concat::array
// array() and view() are separate because std::string_view is non-owning (just a pointer).
// concat::array() returns a temporary, which is destroyed at the end of the expression.
// A string_view to that temporary would dangle.
// The array must persist for the lifetime of its string_view.
// One (the best?) way to do that is by making it a static constexpr object.
template<std::size_t size> constexpr std::string_view view(const std::array<char, size> &array) {
  return {array.data(), size - 1};
}

}  // namespace concat