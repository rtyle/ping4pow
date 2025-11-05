#pragma once

#include <memory>

namespace raii {

// Implements RAII for resources with unary acquisition and release functions.
// The acquirer is called with a pointer to the default-constructed encapsulated resource.
// The releaser will not be called if the acquirer throws an exception.
// implicit accessor operators are provided to the encapsulated resource
// as well as explicit get methods.
template<typename Resource, typename Acquirer, typename Releaser>
  requires std::default_initializable<Resource>
class Raii {
 private:
  Resource resource_;
  std::unique_ptr<Resource, Releaser> releaser_;

 public:
  Raii(Acquirer &&acquirer, Releaser &&releaser)
    : resource_{}
    , releaser_{(acquirer(&resource_), &resource_), std::move(releaser)}
  {}

  // delete unsafe default copy and move constructors
  Raii(const Raii &) = delete;
  Raii &operator=(const Raii &) = delete;
  Raii(Raii &&that) = delete;
  Raii &operator=(Raii &&that) = delete;

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

template<typename R>
struct UnaryFunctionArg;

template<typename R, typename T>
struct UnaryFunctionArg<R(*)(T)> {
  using type = std::remove_pointer_t<T>;
};

template<typename R, typename T>
struct UnaryFunctionArg<R(&)(T)> {
  using type = std::remove_pointer_t<T>;
};

// make an Raii with automatic Resource type deduction from acquirer's unary argument
template<typename Acquirer, typename Releaser>
auto make(Acquirer&& acquirer, Releaser&& releaser) {
    using F = decltype(+std::forward<Acquirer>(acquirer));  // + forces pointer
    using T = typename UnaryFunctionArg<F>::type;
    return Raii<T, std::decay_t<Acquirer>, std::decay_t<Releaser>>{
        std::forward<Acquirer>(acquirer),
        std::forward<Releaser>(releaser)
    };
}

// alias of the Resource type from an Raii we could make from an acquirer
template<typename T>
using Resource = decltype(make(std::declval<void(*)(T*)>(), std::declval<void(*)(T*)>()));

}  // namespace raii