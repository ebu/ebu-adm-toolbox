#pragma once

#include <nlohmann/json.hpp>
#include <sstream>

namespace eat::config_file {
// when parsing the config file, generally elements should be removed when
// they are parsed, so that we can check that all keys were valid

/// get and remove key
template <typename T>
T get(nlohmann::json &config, const std::string &key) {
  T value = config.at(key).get<T>();
  config.erase(key);
  return value;
}

template <typename T>
std::optional<T> get_optional(nlohmann::json &config, const std::string &key) {
  std::optional<T> value;
  if (config.contains(key)) {
    value = config[key];
    config.erase(key);
  }
  return value;
}

/// get and remove key, or return default_value if it doesn't exist
template <typename T>
T get(nlohmann::json &config, const std::string &key, const T &default_value) {
  if (config.contains(key))
    return get<T>(config, key);
  else
    return default_value;
}

inline nlohmann::json get_subobject(nlohmann::json &config, const std::string &key) {
  nlohmann::json value = config.at(key);
  if (!value.is_object()) throw std::runtime_error("expected " + key + " to be an object");
  config.erase(key);
  return value;
}

/// check that config is empty (because all keys have been removed)
inline void check_empty(nlohmann::json config) {
  config.erase("$schema");

  if (!config.empty()) {
    std::ostringstream s;
    s << "unused keys:";
    for (const auto &pair : config.get<typename nlohmann::json::object_t>()) {
      s << " " << pair.first;
    }

    throw std::runtime_error{s.str()};
  }
}
}  // namespace eat::config_file
