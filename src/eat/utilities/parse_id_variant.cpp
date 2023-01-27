#include "eat/utilities/parse_id_variant.hpp"

#include <string_view>

namespace eat::utilities {
adm::ElementIdVariant parse_id_variant(const std::string &id) {
  size_t underscore_pos = id.find('_');
  if (underscore_pos == std::string::npos) throw std::runtime_error("could not parse id " + id);

  std::string_view type = std::string_view{id}.substr(0, underscore_pos);

  if (type == "ATU") return adm::parseAudioTrackUidId(id);
  if (type == "AP") return adm::parseAudioPackFormatId(id);
  if (type == "AC") return adm::parseAudioChannelFormatId(id);
  if (type == "AS") return adm::parseAudioStreamFormatId(id);
  if (type == "AT") return adm::parseAudioTrackFormatId(id);
  if (type == "APR") return adm::parseAudioProgrammeId(id);
  if (type == "ACO") return adm::parseAudioContentId(id);
  if (type == "AO")
    return adm::parseAudioObjectId(id);
  else
    throw std::runtime_error("unknown ID type: " + id);
}
}  // namespace eat::utilities
