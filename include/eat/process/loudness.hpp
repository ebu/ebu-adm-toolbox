#include <ear/layout.hpp>

#include "adm/elements_fwd.hpp"
#include "eat/framework/process.hpp"

namespace eat::process {
/// a process which measures the loudness of input samples
/// - in_samples (StreamPort<InterleavedBlockPtr>) : input samples
/// - out_loudness (DataPort<adm::LoudnessMetadata>) : measured loudness
framework::ProcessPtr make_measure_loudness(const std::string &name, const ear::Layout &layout);

/// a process which sets the loudness of an audioProgramme with the given ID
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - in_loudness (DataPort<adm::LoudnessMetadata>) : measured loudness
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_set_programme_loudness(const std::string &name, const adm::AudioProgrammeId &programme_id);

/// a process which measures the loudness of all audioProgrammes (by rendering
/// them to 4+5+0) and updates the axml to match
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - in_samples (StreamPort<InterleavedBlockPtr>) : input samples for in_axml
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_update_all_programme_loudnesses(const std::string &name);
}  // namespace eat::process
