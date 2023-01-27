#pragma once
#include <memory>

namespace eat::utilities {

namespace detail {
template <typename T>
struct UnwrapShared {
  template <typename U>
  U operator()(U &&value) {
    return value;
  }

  using type = T;
};

template <typename T>
struct UnwrapShared<std::shared_ptr<T>> {
  template <typename U>
  auto &&operator()(U &&value) {
    return *value;
  }

  using type = T;
};

}  // namespace detail

/// return *value if it is a shared_ptr, or value otherwise
template <typename T>
auto unwrap_shared(T &&value) -> decltype(detail::UnwrapShared<std::remove_cvref_t<T>>{}(std::forward<T>(value))) {
  return detail::UnwrapShared<std::remove_cvref_t<T>>{}(std::forward<T>(value));
}

}  // namespace eat::utilities
