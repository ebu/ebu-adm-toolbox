#pragma once
#include <adm/elements/audio_content_id.hpp>
#include <adm/elements/audio_object_id.hpp>
#include <adm/elements/audio_programme_id.hpp>

#include "rendering_items.hpp"

namespace eat::render {

// same as SelectionOptions, but with IDs instead of references

using ProgrammeIdStart = adm::AudioProgrammeId;
using ContentIdStart = std::vector<adm::AudioContentId>;
using ObjectIdStart = std::vector<adm::AudioObjectId>;

using SelectionStartId = std::variant<DefaultStart, ProgrammeIdStart, ContentIdStart, ObjectIdStart>;

struct SelectionOptionsId {
  SelectionOptionsId() = default;
  SelectionOptionsId(SelectionStartId start_);

  SelectionStartId start = DefaultStart{};
};

SelectionOptions selection_options_from_ids(const std::shared_ptr<adm::Document> &doc,
                                            const SelectionOptionsId &options);

}  // namespace eat::render
