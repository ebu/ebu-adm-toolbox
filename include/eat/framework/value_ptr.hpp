#pragma once
#include <memory>

namespace eat::framework {

/// extension point for copying data stored in a shared_ptr, for types that are
/// always stored in shared_ptrs and therefore don't have a copy constructor
template <typename T>
std::shared_ptr<T> copy_shared_ptr(const std::shared_ptr<T> &value) {
  return std::make_shared<T>(*value);
}

/// a wrapper around shared_ptr that has more value-like semantics while
/// avoiding copies where possible
///
/// this should be used in ports (or structures moved through ports) to wrap
/// things like ADM data which the user might want to modify in-place but are
/// expensive to copy
///
/// the value can not be modified in-place, as this would be visible in other
/// 'copies' of this structure
template <typename T>
class ValuePtr {
 public:
  ValuePtr() {}
  ValuePtr(std::shared_ptr<T> value_) : value(std::move(value_)) {}

  /// get read-only access to the value
  std::shared_ptr<const T> read() const { return value; }

  /// get a non-const value that can be modified
  ///
  /// this makes a copy if there are multiple users of the underlying value, or
  /// moves if there is only one
  std::shared_ptr<T> move_or_copy() const {
    if (value.use_count() > 1)
      return copy_shared_ptr(value);
    else
      return std::move(value);
  }

 private:
  std::shared_ptr<T> value;
};

}  // namespace eat::framework
