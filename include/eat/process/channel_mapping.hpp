#pragma once
#include <vector>

#include "eat/framework/process.hpp"

namespace eat::process {

/// instruction for remapping audio channels
///
/// the size of this is the number of elements in the output, and `cm[output_channel] == input_channel`
using ChannelMapping = std::vector<size_t>;

/// apply a ChannelMapping to some samples
///
/// this can be used to rearrange or remove channels
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - in_channel_mapping (DataPort<ChannelMapping>) : channel mapping to apply
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_apply_channel_mapping(const std::string &name);

}  // namespace eat::process
