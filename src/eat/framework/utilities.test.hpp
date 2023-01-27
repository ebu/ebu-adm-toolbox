#pragma once
#include <string>
#include <vector>

namespace eat::framework {

/// make a string line name(args[0], args[1]...)
inline std::string format_str(const std::string &name, const std::vector<std::string> &args) {
  std::string out = name + "(";
  for (size_t i = 0; i < args.size(); i++) {
    if (i > 0) out += ", ";
    out += args[i];
  }
  return out + ")";
}

}  // namespace eat::framework
