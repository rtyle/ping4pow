#pragma once

#include <functional>
#include <memory>

namespace raii {

// Raii implements RAII for a Resource with unary Acquirer and Releaser callables
// that will be invoked with a pointer to its default_initializable Resource member.
// The Acquirer is invoked upon construction and, if successful, the Releaser
// will be invoked upon destruction.
// Implicit accessor operators are provided to the protected Resource member
// as well as explicit get methods.
template<typename Resource, typename Acquirer = void (*)(Resource *), typename Releaser = void (*)(Resource *)>
  requires std::default_initializable<Resource>
class Raii {
 protected:
  Resource resource_;
  std::unique_ptr<Resource, Releaser> releaser_;

 public:
  Raii(Acquirer &&acquirer, Releaser &&releaser)
      : resource_{},
        releaser_{(std::invoke(std::forward<Acquirer>(acquirer), &resource_), &resource_), std::move(releaser)} {}

  // delete unsafe default copy and move constructors
  Raii(const Raii &) = delete;
  Raii &operator=(const Raii &) = delete;
  Raii(Raii &&that) = delete;
  Raii &operator=(Raii &&that) = delete;

  // non-pointer Resource: act like a reference
  operator Resource &()
    requires(!std::is_pointer_v<Resource>)
  {
    return resource_;
  }
  operator const Resource &() const
    requires(!std::is_pointer_v<Resource>)
  {
    return resource_;
  }
  operator Resource *()
    requires(!std::is_pointer_v<Resource>)
  {
    return &resource_;
  }

  // pointer Resource: smart-pointer-like
  auto operator->() const
    requires std::is_pointer_v<Resource>
  {
    return resource_;
  }
  decltype(auto) operator*() const
    requires std::is_pointer_v<Resource>
  {
    return *resource_;
  }

  Resource &get() { return this->resource_; }
  const Resource &get() const { return this->resource_; }
};

// ---- generic unary callable argument type deduction

// primary template for specializations (the base for SFINAE)
template<typename Callable> struct deduce_argument;

// mutable lambda or non-const (default) function object
template<typename R, typename C, typename A> struct deduce_argument<R (C::*)(A)> {
  using type = std::remove_pointer_t<A>;
};

// immutable (default) lambda or const function object
template<typename R, typename C, typename A> struct deduce_argument<R (C::*)(A) const> {
  using type = std::remove_pointer_t<A>;
};

// function pointer (function references decay to this)
template<typename R, typename A> struct deduce_argument<R (*)(A)> {
  using type = std::remove_pointer_t<A>;
};

// Callable lambda or function object, deduce_argument from what it decays to
template<typename Callable> struct deduce_argument {
  using type = typename deduce_argument<decltype(&std::decay_t<Callable>::operator())>::type;
};

// Callable, deduce_argument from what it decays to
template<typename Callable> using deduce_argument_t = typename deduce_argument<std::decay_t<Callable>>::type;

// factory
template<typename Acquirer, typename Releaser> auto make(Acquirer &&acquirer, Releaser &&releaser) {
  using Resource = deduce_argument_t<Acquirer>;
  return Raii<Resource, std::decay_t<Acquirer>, std::decay_t<Releaser>>{std::forward<Acquirer>(acquirer),
                                                                        std::forward<Releaser>(releaser)};
}

}  // namespace raii