#pragma once
#include <functional>

#include "eat/framework/process.hpp"
#include "eat/process/profiles.hpp"

namespace eat::utilities {

/// check that two streams of blocks represent the same samples, with some tolerance
///
/// the tolerance allowed is abs(reference sample) * rtol + atol
///
/// an exception will be thrown if something is different:
/// - sample rate
/// - number of samples
/// - number of channels
/// - samples not within tolerance
///
/// ports:
/// - in_samples_ref (StreamPort<InterleavedBlockPtr>) : reference input samples
/// - in_samples_test (StreamPort<InterleavedBlockPtr>) : test input samples
framework::ProcessPtr make_check_samples(const std::string &name, float rtol, float atol,
                                         std::function<void(const std::string &)> error_cb);

}  // namespace eat::utilities
