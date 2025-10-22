#pragma once

#include <concepts>
#include <type_traits>
#include <utility>
#include <functional>
#include <stdexcept>

namespace raii {

// Implements RAII for resources with unary acquisition and release functions.
// The acquirer is called with a pointer to the default-constructed resource.
// The acquirer must throw on failure to ensure the releaser is only called
// on a valid resource during destruction.
template<typename Resource, typename Acquirer, typename Releaser>
  requires std::default_initializable<Resource>
class Raii {
 private:
  Resource resource_;
  Releaser releaser_;
  bool valid_;

 public:
  Raii(Acquirer &&acquirer, Releaser &&releaser) : resource_{}, releaser_(std::move(releaser)), valid_(true) {
    acquirer(&this->resource_);
  }

  ~Raii() { this->release(); }

  void release() {
    if (this->valid_) {
      this->releaser_(&this->resource_);
      this->valid_ = false;
    }
  }

  Raii(const Raii &) = delete;
  Raii &operator=(const Raii &) = delete;

  Raii(Raii &&that) noexcept
      : resource_(std::move(that.resource_)), releaser_(std::move(that.releaser_)), valid_(that.valid_) {
    that.valid_ = false;
  }

  Raii &operator=(Raii &&that) noexcept {
    if (this != &that) {
      if (this->valid_) {
        this->releaser_(this->resource_);
      }
      this->resource_ = std::move(that.resource_);
      this->releaser_ = std::move(that.releaser_);
      this->valid_ = that.valid_;
      that.valid_ = false;
    }
    return *this;
  }

  template<typename R = Resource>
    requires(!std::is_pointer_v<R>)
  operator R &() {
    return this->resource_;
  }

  template<typename R = Resource>
    requires(!std::is_pointer_v<R>)
  operator const R &() const {
    return this->resource_;
  }

  template<typename R = Resource>
    requires(!std::is_pointer_v<R>)
  operator R *() {
    return &this->resource_;
  }

  template<typename R = Resource>
    requires std::is_pointer_v<R>
  auto operator->() const {
    return this->resource_;
  }

  template<typename R = Resource>
    requires std::is_pointer_v<R>
  decltype(auto) operator*() const {
    return *this->resource_;
  }

  Resource &get() { return this->resource_; }
  const Resource &get() const { return this->resource_; }
};

// Helper to extract T from unary function pointer void (*)(T*)
template<typename F> struct UnaryFunctionArg;

template<typename T> struct UnaryFunctionArg<void (*)(T *)> {
  using Type = T;
};

template<typename Acquirer, typename Releaser> auto make(Acquirer &&acquirer, Releaser &&releaser) {
  using T = typename UnaryFunctionArg<std::decay_t<Acquirer>>::Type;
  return Raii<T, std::decay_t<Acquirer>, std::decay_t<Releaser>>(std::forward<Acquirer>(acquirer),
                                                                 std::forward<Releaser>(releaser));
}

template<typename T> using Resource = decltype(make(std::declval<void (*)(T *)>(), std::declval<void (*)(T *)>()));

}  // namespace raii