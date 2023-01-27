#pragma once
#include "eat/framework/process.hpp"

namespace eat::process {
/// a process which removes unreferenced elements from an ADM document, and
/// re-packs the channels to remove unreferenced channels
///
/// ports:
/// - in_samples (StreamPort<InterleavedBlockPtr>) : input samples
/// - out_samples (StreamPort<InterleavedBlockPtr>) : output samples
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_remove_unused(const std::string &name);

/// a process which removes unreferenced elements from an ADM document
///
/// in contrast with make_remove_unused, this doesn't do anything with the
/// audio, so can be useful if previous changes will not have affected the
/// use of channels
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_remove_unused_elements(const std::string &name);
}  // namespace eat::process
