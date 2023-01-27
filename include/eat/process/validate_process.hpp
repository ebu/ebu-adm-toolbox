#pragma once
#include "eat/framework/process.hpp"
#include "eat/process/profiles.hpp"

namespace eat::process {
/// a process which takes an ADM document, checks it against a profile, and
/// prints any errors then raises an exception if any issues are found
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
framework::ProcessPtr make_validate(const std::string &name, const profiles::Profile &profile);
}  // namespace eat::process
