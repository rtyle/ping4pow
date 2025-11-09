#include <functional>
#include <memory>

namespace raii {

// ---- generic unary callable argument type deduction

// primary template for specializations (the base for SFINAE)
template<typename Callable> struct deduce_argument;

// mutable lambda or non-const (default) function object
template<typename R, typename C, typename T> struct deduce_argument<R (C::*)(T)> {
  using type = T;
};

// immutable (default) lambda or const function object
template<typename R, typename C, typename T> struct deduce_argument<R (C::*)(T) const> {
  using type = T;
};

// function pointer (function references decay to this)
template<typename R, typename T> struct deduce_argument<R (*)(T)> {
  using type = T;
};

// Callable lambda or function object, deduce_argument from what it decays to
template<typename Callable> struct deduce_argument {
  using type = typename deduce_argument<decltype(&std::decay_t<Callable>::operator())>::type;
};

// deduce_argument_t of object pointed to from what Callable decays to
template<typename Callable>
using deduce_argument_t = std::remove_pointer_t<typename deduce_argument<std::decay_t<Callable>>::type>;

// raii:Resource is a thin wrapper around a type T
// that adds C++ RAII behavior to T using related
// Acquirer and Releaser callables that expect a pointer to this T instance.
// The Acquirer is invoked upon construction and, if successful, the Releaser
// will be invoked upon destruction.
template<typename T, typename Acquirer = void (*)(T *), typename Releaser = void (*)(T *)>
  requires std::default_initializable<T> && std::is_same_v<T, deduce_argument_t<Acquirer>> &&
               std::is_same_v<T, deduce_argument_t<Releaser>>
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
};

// factory
template<typename Acquirer, typename Releaser> auto make(Acquirer &&acquirer, Releaser &&releaser) {
  using T = deduce_argument_t<Acquirer>;
  return Resource<T, std::decay_t<Acquirer>, std::decay_t<Releaser>>{std::forward<Acquirer>(acquirer),
                                                                     std::forward<Releaser>(releaser)};
}

}  // namespace raii