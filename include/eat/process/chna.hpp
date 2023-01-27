#pragma once
#include <adm/elements_fwd.hpp>
#include <cstddef>
#include <map>

/// forward declarations
namespace bw64 {
class ChnaChunk;
}

namespace eat::process {

/// mapping from audioTrackUids to zero-based channel numbers in the associated stream/file
using channel_map_t = std::map<adm::AudioTrackUidId, size_t>;

/// add information from a CHNA chunk into an ADM document and channel map
void load_chna(adm::Document &document, channel_map_t &channel_map, const bw64::ChnaChunk &chna);

/// make a CHNA chunk for an ADM document and channel map
bw64::ChnaChunk make_chna(const adm::Document &document, const channel_map_t &channel_map);

}  // namespace eat::process
