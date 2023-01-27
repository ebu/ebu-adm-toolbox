#pragma once
#include "eat/framework/process.hpp"
#include "eat/process/profiles.hpp"

namespace eat::process {

// miscellaneous processes which may be useful when converting between
// different profiles

/// set the list of profiles that this document should conform to
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_set_profiles(const std::string &name, const std::vector<profiles::Profile> &profiles);

/// set position defaults
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_set_position_defaults(const std::string &name);

/// replace silent audioTrackUID references in audioObjects with a real track
/// that references a silent channel
///
/// ports:
/// - in_samples (StreamPort<InterleavedBlockPtr>) : input samples
/// - out_samples (StreamPort<InterleavedBlockPtr>) : output samples
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_remove_silent_atu(const std::string &name);

/// remove time/duration from audioObjects where it is safe to do so (doesn't
/// potentially change the rendering) and can be done by only changing the
/// metadata (no audio changes, no converting common definitions
/// audioChannelFormats to real audioChannelFormats)
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_remove_object_times_data_safe(const std::string &name);

/// remove start and duration from audioObjects which only reference common
/// definitions audioChannelFormats
///
/// this could cause rendering changes if there are non-zero samples outside
/// the range of the audioObject, but should be safe on EPS output
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_remove_object_times_common_unsafe(const std::string &name);

/// remove importance values from all audioObjects, audioPackFormats and
/// audioBlockFormats
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_remove_importance(const std::string &name);

/// rewrite the programme-content-object structure to make it compatible with
/// emission profile rules
///
/// this may drop audioContents or audioObjects that are too nested (only ones
/// that have audioObject references), so any information which applies to the
/// audioObjects below will be lost
///
/// max_objects_depth is the maximum nesting depth of any object, which is defined for an object as:
/// - 0 for objects which do not contain object references
/// - the maximum object depth of any referenced objects, plus 1
/// for example, of this is 0, object nesting is removed
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_rewrite_content_objects_emission(const std::string &name, int max_objects_depth = 2);

/// ensure that all audioObjects have an interact parameter based on the
/// presence or absence of the audioObjectInteraction element
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_infer_object_interact(const std::string &name);

/// set missing audioContent dialogue values to mixed
///
/// ports:
/// - in_axml (DataPort<ADMData>) : input ADM data
/// - out_axml (DataPort<ADMData>) : output ADM data
framework::ProcessPtr make_set_content_dialogue_default(const std::string &name);

}  // namespace eat::process
