#pragma once
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace eat::process::validation::detail {
// implementation of ToMessages, which makes a variant of messages given a
// variant of checks, where the message types aredetermined by looking at the
// return type of check.run(), which can be either a single message or a
// variant

/// unconstrained list of types with no functionality
template <typename... T>
struct TypeList {};

/// does a TypeList Ts contain U?
template <typename Ts, typename U>
struct Contains;

template <typename T, typename... Ts, typename U>
struct Contains<TypeList<T, Ts...>, U>
    : std::conditional_t<std::is_same_v<T, U>, std::true_type, Contains<TypeList<Ts...>, U>> {};

template <typename U>
struct Contains<TypeList<>, U> : std::false_type {};

/// add U to TypeList Ts if it is not already present
template <typename Ts, typename U>
struct AddUnique;

template <typename... Ts, typename U>
struct AddUnique<TypeList<Ts...>, U> {
  using type = std::conditional_t<Contains<TypeList<Ts...>, U>::value, TypeList<Ts...>, TypeList<U, Ts...>>;
};

/// add types from TypeList Us to TypeList Ts if they are not already present
template <typename Ts, typename Us>
struct AddUniqueList;

template <typename Ts, typename U, typename... Us>
struct AddUniqueList<Ts, TypeList<U, Us...>> {
  using type = typename AddUnique<typename AddUniqueList<Ts, TypeList<Us...>>::type, U>::type;
};

template <typename Ts>
struct AddUniqueList<Ts, TypeList<>> {
  using type = Ts;
};

/// turn various types into a TypeList
/// std::variant<Ts...> -> TypeList<Ts...>
/// std::vector<std::variant<Ts...>> -> TypeList<Ts...>
/// std::vector<T> -> TypeList<T>
template <typename T>
struct ToTypeList;

template <typename... Ts>
struct ToTypeList<std::variant<Ts...>> {
  using type = TypeList<Ts...>;
};

template <typename... Ts>
struct ToTypeList<std::vector<std::variant<Ts...>>> {
  using type = TypeList<Ts...>;
};

template <typename T>
struct ToTypeList<std::vector<T>> {
  using type = TypeList<T>;
};

/// the message types returned by a check, as a TypeList, based on the result
/// type of run(), which should either return a vector of messages or message
/// variants
template <typename Check>
using MessageTypes = typename ToTypeList<decltype(std::declval<Check>().run(std::declval<ADMData>()))>::type;

/// turn a TypeList if Checks into a TypeList of unique messages they could return
template <typename Checks>
struct ToMessagesHelper;

template <typename Check, typename... Checks>
struct ToMessagesHelper<TypeList<Check, Checks...>> {
  using type = typename AddUniqueList<typename ToMessagesHelper<TypeList<Checks...>>::type, MessageTypes<Check>>::type;
};

template <>
struct ToMessagesHelper<TypeList<>> {
  using type = TypeList<>;
};

/// turn a TypeList into a std::variant with the same types
template <typename T>
struct TypeListToVariant;

template <typename... Ts>
struct TypeListToVariant<TypeList<Ts...>> {
  using type = std::variant<Ts...>;
};

/// given a std::variant of checks, get a std::variant of messages which could be returned
template <typename Checks>
using ToMessages = typename detail::TypeListToVariant<
    typename detail::ToMessagesHelper<typename detail::ToTypeList<Checks>::type>::type>::type;

template <typename Variant, typename T>
struct ToVariantHelper {
  template <typename U>
  Variant operator()(U &&value) {
    return Variant{std::forward<U>(value)};
  }
};

template <typename Variant, typename... Ts>
struct ToVariantHelper<Variant, std::variant<Ts...>> {
  template <typename U>
  Variant operator()(U &&value) {
    return std::visit([](auto &&value_t) { return Variant{std::forward<decltype(value_t)>(value_t)}; },
                      std::forward<U>(value));
  }
};

/// convert a value to the given variant type
/// value can be a type in Variant, or another variant with a subset of the
/// types of Variant
template <typename Variant, typename T>
Variant to_variant(T &&value) {
  return ToVariantHelper<Variant, T>{}(std::forward<T>(value));
}

template <typename T>
struct TypeName;

template <>
struct TypeName<std::string> {
  static constexpr std::string_view name = "string";
};

template <>
struct TypeName<float> {
  static constexpr std::string_view name = "float";
};

/// get a unique and readable name for a type
///
/// type_info could be used for this, but it requires some work to post-process
/// (de-mangling and removing namespaces)
template <typename T>
const std::string type_name = std::string{TypeName<T>::name};

}  // namespace eat::process::validation::detail
