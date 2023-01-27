#pragma once
#include <adm/element_variant.hpp>

#include "eat/framework/process.hpp"

namespace eat::process {

using ElementIds = std::vector<adm::ElementIdVariant>;

/// a process which removes the given elements
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_remove_elements(const std::string &name, ElementIds ids);
}  // namespace eat::process
