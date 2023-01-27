#pragma once
#include "eat/framework/process.hpp"

namespace eat::process {
/// add frequency information to DirectSpeakers blocks with LFE in their name
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_fix_ds_frequency(const std::string &name);

/// fix audioBlockFormat durations to match up with the next rtimes
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_fix_block_durations(const std::string &name);

/// remove audioPackFormatIDRef in audioStreamFormats that are of type PCM and
/// have an audioChannelFormatIDRef
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_fix_stream_pack_refs(const std::string &name);

/// replace
/// audioTrackUid->audioTrackFormat->audioStreamFormat->audioChannelFormat
/// references with audioTrackUid->audioChannelFormat references
///
/// this doesn't remove any unused elements, so use RemoveUnusedElements after
/// this
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_convert_track_stream_to_channel(const std::string &name);

/// ensure that blocks with a specified duration have an rtime
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_add_block_rtimes(const std::string &name);

/// set the audioFormatExtended version
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_set_version(const std::string &name, const std::string &version);

}  // namespace eat::process
