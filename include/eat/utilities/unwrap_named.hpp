#pragma once

#include <adm/detail/named_type.hpp>

namespace eat::utilities {

namespace detail {
template <typename T>
struct UnwrapNamedType {
  template <typename U>
  U operator()(U &&value) {
    return value;
  }

  using type = T;
};

template <typename T, typename Tag, typename stdalidator>
struct UnwrapNamedType<adm::detail::NamedType<T, Tag, stdalidator>> {
  template <typename U>
  auto &&operator()(U &&value) {
    return value.get();
  }

  using type = T;
};
}  // namespace detail

/// get the inner type of a NamedType, or pass through T
template <typename T>
using unwrap_named_t = typename detail::UnwrapNamedType<T>::type;

/// unwrap a NamedType, or pass through a non-NamedType value
///
/// return type is the same as value or value.get(), but the trailing return
/// type is needed to add const& in the case where T is not a NamedType and
/// value is a reference
template <typename T>
auto unwrap_named(T &&value) -> decltype(detail::UnwrapNamedType<std::remove_cvref_t<T>>{}(std::forward<T>(value))) {
  return detail::UnwrapNamedType<std::remove_cvref_t<T>>{}(std::forward<T>(value));
}

}  // namespace eat::utilities
