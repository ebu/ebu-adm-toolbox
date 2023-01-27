//
// Created by Richard Bailey on 22/11/2022.
//
#pragma once

#include <nlohmann/json_fwd.hpp>
#include <ostream>

namespace eat::process {
void validate_config(nlohmann::json const &config, std::ostream &err);

}
