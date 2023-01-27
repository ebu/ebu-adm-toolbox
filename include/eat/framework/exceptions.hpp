#pragma once
#include <stdexcept>

namespace eat::framework {

class AssertionError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

inline void always_assert(bool condition, const std::string &message) {
  if (!condition) throw AssertionError(message);
}

class ValidationError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

}  // namespace eat::framework
