#pragma once
#include <nlohmann/json.hpp>

#include "eat/framework/process.hpp"

namespace eat::config_file {
framework::Graph make_graph(nlohmann::json config);
framework::ProcessPtr make_process(nlohmann::json &config);
}  // namespace eat::config_file
