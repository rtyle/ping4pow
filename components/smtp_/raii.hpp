#include <functional>
#include <memory>

namespace raii {

namespace detail {

// ---- generic unary callable argument type deduction

// primary template for specializations (the base for SFINAE)
template<typename Callable> struct deduced_argument;

// mutable lambda or non-const (default) function object
template<typename R, typename C, typename T> struct deduced_argument<R (C::*)(T *)> {
  using type = T;
};

// immutable (default) lambda or const function object
template<typename R, typename C, typename T> struct deduced_argument<R (C::*)(T *) const> {
  using type = T;
};

// function pointer (function references decay to this)
template<typename R, typename T> struct deduced_argument<R (*)(T *)> {
  using type = T;
};

// Callable lambda or function object, deduced_argument from what it decays to
template<typename Callable> struct deduced_argument {
  using type = typename deduced_argument<decltype(&std::decay_t<Callable>::operator())>::type;
};

// a Steward is a unary Callable that takes a resource type pointer
template<typename Callable, typename T>
concept Steward = std::is_invocable_v<Callable, T *>;

// raii::Stewardship requires a default_initializable type
// with an Acquirer and Releaser Steward of this type
template<typename T, typename Acquirer, typename Releaser>
concept Stewardship = std::default_initializable<T> && Steward<Acquirer, T> && Steward<Releaser, T>;

}  // namespace detail

// raii:Resource is a thin wrapper around a type T
// whose T, Acquirer and Releaser meet Stewardship requirements.
// The Acquirer is invoked upon construction and, if successful, the Releaser
// will be invoked upon destruction.
template<typename T, typename Acquirer = void (*)(T *), typename Releaser = void (*)(T *)>
  requires detail::Stewardship<T, Acquirer, Releaser>
class Resource : public T, protected std::unique_ptr<T, Releaser> {
 public:
  Resource(Acquirer &&acquirer, Releaser &&releaser)
      : T{},
        std::unique_ptr<T, Releaser>{(std::invoke(std::forward<Acquirer>(acquirer), this), this), std::move(releaser)} {
  }

  // delete unsafe default copy and move constructors
  Resource(const Resource &) = delete;
  Resource &operator=(const Resource &) = delete;
  Resource(Resource &&that) = delete;
  Resource &operator=(Resource &&that) = delete;

  ~Resource() = default;
};

// factory
template<typename Acquirer, typename Releaser> auto make(Acquirer &&acquirer, Releaser &&releaser) {
  using T = typename detail::deduced_argument<std::decay_t<Acquirer>>::type;
  return Resource<T, std::decay_t<Acquirer>, std::decay_t<Releaser>>{std::forward<Acquirer>(acquirer),
                                                                     std::forward<Releaser>(releaser)};
}

}  // namespace raii
