#pragma once
#include <stdexcept>
#include <variant>

namespace eat::process::profiles {

struct ITUEmissionProfile {
 public:
  ITUEmissionProfile(int level) : level_(level) {
    if (level_ < 0 || level_ > 2) throw std::invalid_argument{"level must be 0, 1 or 2"};
  }

  int level() const { return level_; }

 private:
  int level_;
};

/// represents some known profile, used to select behaviours for different
/// profiles, for example when validating or conforming files
using Profile = std::variant<ITUEmissionProfile>;
}  // namespace eat::process::profiles
