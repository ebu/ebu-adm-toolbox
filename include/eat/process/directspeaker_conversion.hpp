//
// Created by Richard Bailey on 05/08/2022.
//
#pragma once
#include <adm/elements_fwd.hpp>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "eat/process/adm_bw64.hpp"

namespace eat::process {
struct CartesianPosition {
  float x;
  float y;
  float z;
};

namespace detail {
struct SpeakerData {
  std::string audioChannelFormatName;
  std::string speakerLabel;
  CartesianPosition pos;
};

const SpeakerData Left{"RoomCentricLeft", "RC_L", {-1.0, 1.0, 0.0}};
const SpeakerData Right{"RoomCentricRight", "RC_R", {1.0, 1.0, 0.0}};
const SpeakerData Center{"RoomCentricCenter", "RC_C", {0.0, 1.0, 0.0}};
const SpeakerData LFE{"RoomCentricLFE", "RC_LFE", {-1.0, 1.0, -1.0}};
const SpeakerData LeftSurround{"RoomCentricLeftSurround", "RC_Ls", {-1.0, -0.363970, 0.0}};
const SpeakerData RightSurround{"RoomCentricRightSurround", "RC_Rs", {1.0, -0.363970, 0.0}};
const SpeakerData LeftSideSurround{"RoomCentricLeftSideSurround", "RC_Lss", {-1.0, 0.0, 0.0}};
const SpeakerData RightSideSurround{"RoomCentricRightSideSurround", "RC_Rss", {1.0, 0.0, 0.0}};
const SpeakerData LeftRearSurround{"RoomCentricLeftRearSurround", "RC_Lrs", {-1.0, -1.0, 0.0}};
const SpeakerData RightRearSurround{"RoomCentricRightRearSurround", "RC_Rrs", {1.0, -1.0, 0.0}};
const SpeakerData LeftTopSurround{"RoomCentricLeftTopSurround", "RC_Lts", {-1.0, 0.0, 1.0}};
const SpeakerData RightTopSurround{"RoomCentricRightTopSurround", "RC_Rts", {1.0, 0.0, 1.0}};

/* TODO: add all packs */
/* TODO: deal with zeroing non existing buffers */
const std::map<std::string, std::vector<SpeakerData>> common_pack_to_atmos_speaker_map{
    /* 2.0 */
    {"AP_00010002", {Left, Right}},
    /* 3.0 */
    {"AP_0001000a", {Left, Right, Center}},
    /* 5.0 */
    {"AP_0001000c", {Left, Right, Center, LeftSurround, RightSurround}},
    /* 5.1 */
    {"AP_00010003", {Left, Right, Center, LFE, LeftSurround, RightSurround}},
    /* 7.1back */
    {"AP_0001000f",
     {Left, Right, Center, LFE, LeftSideSurround, RightSideSurround, LeftRearSurround, RightRearSurround}},
    /* 7.1.2*/
    {"AP_00010016",
     {Left, Right, Center, LFE, LeftSideSurround, RightSideSurround, LeftRearSurround, RightRearSurround,
      LeftTopSurround, RightTopSurround}},
};

struct DSChannel {
  std::string audioChannelFormatName;
  std::shared_ptr<adm::AudioChannelFormat> channel;
  std::shared_ptr<adm::AudioTrackFormat> track;
  std::shared_ptr<adm::AudioStreamFormat> stream;
};
using PackConversionLookup = std::map<std::string, std::vector<SpeakerData>>;
struct ConvertibleLayout {
  std::vector<std::shared_ptr<adm::AudioTrackUid>> uids;
  std::vector<SpeakerData> target_layout;
};
using TargetLayoutMap = std::unordered_map<std::shared_ptr<adm::AudioPackFormat>, ConvertibleLayout>;
struct MappedPack {
  std::shared_ptr<adm::AudioPackFormat> target;
  std::vector<DSChannel> elements;
};
using MappedPacks = std::unordered_map<std::shared_ptr<adm::AudioPackFormat>, MappedPack>;
using MappedUids = std::unordered_map<std::shared_ptr<adm::AudioTrackUid>, std::shared_ptr<adm::AudioTrackUid>>;

/*
 * For each AudioPackFormat in the input document that
 *  a) is referenced by an AudioTrackUID in the document
 *  b) has a pack ID that is a key in the provided layoutMap
 * Return a mapping from that pack format to a target speaker layout
 */
TargetLayoutMap find_target_layouts(adm::Document &document, PackConversionLookup const &layoutMap);

/*
 * Returns a set of all speakers used in the mapped target layouts
 */
std::vector<SpeakerData> mapped_speaker_set(TargetLayoutMap const &layouts);

/*
 * For each speaker provided, create an AudioChannelFormat, AudioTrackFormat, and AudioStreamFormat that reference each
 * other.
 */
std::vector<DSChannel> convert_speakers(std::vector<SpeakerData> const &speakerData);

/*
 * For each target layout, return a mapping from the original AudioPackFormat, to a target AudioPackFormat and vector of
 * objects that represent speakers in that layout (AudioChannelFormat, AudioTrackFormat, AudioStreamFormat)
 */
MappedPacks create_converted_packs(TargetLayoutMap const &layoutMap, std::vector<DSChannel> const &convertedChannels);

/*
 * For every uid in the document, if it references a mapped pack, create a replacement that references the target pack.
 * Return a mapping from the mapped document uids to the replacement uids
 */
MappedUids create_converted_uids(adm::Document &document, MappedPacks const &packs);

/*
 * For every AudioObject that references a mapped AudioTrackUid, change the reference to point to the replacement uid
 * Remove all mapped uids, add all replacement uids to the document
 * (This also adds their referenced AudioTrackFormat, AudioPackFormat, AudioStreamFormat elements)
 * For mapped AudioTrackUids, replace their IDs in the channel map with their replacements.
 */
void replace_layouts(adm::Document &doc, channel_map_t &channel_map, MappedUids const &uids);

/*
 * For every pack format reference in every AudioObject, if the referred pack has been converted, replace the reference
 * with one to the new pack.
 */
void replace_object_pack_references(adm::Document &doc, MappedPacks const &packMap);
}  // namespace detail

}  // namespace eat::process
