#pragma once
#include <adm/elements_fwd.hpp>
#include <memory>

namespace eat::render {

/// given an audioTrackUID, find the associated audioChannelFormat, looking
/// at both the direct reference and the reference via the audioTrackFormat
/// and audioStreamFormat
std::shared_ptr<adm::AudioChannelFormat> channel_format_for_track_uid(const std::shared_ptr<adm::AudioTrackUid> &track);
}  // namespace eat::render
