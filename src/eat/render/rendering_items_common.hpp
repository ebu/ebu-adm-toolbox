#pragma once
#include <adm/elements_fwd.hpp>
#include <memory>

namespace eat::render {

using DocumentPtr = std::shared_ptr<adm::Document>;
using ProgrammePtr = std::shared_ptr<adm::AudioProgramme>;
using ContentPtr = std::shared_ptr<adm::AudioContent>;
using ObjectPtr = std::shared_ptr<adm::AudioObject>;
using ChannelFmtPointer = std::shared_ptr<adm::AudioChannelFormat>;
using PackFmtPointer = std::shared_ptr<adm::AudioPackFormat>;
using TrackUIDPointer = std::shared_ptr<adm::AudioTrackUid>;

}  // namespace eat::render
