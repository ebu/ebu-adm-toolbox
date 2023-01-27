#include "eat/process/directspeaker_conversion.hpp"

#include <adm/common_definitions.hpp>
#include <adm/document.hpp>
#include <adm/elements.hpp>
#include <adm/utilities/object_creation.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace adm;
using namespace eat::process::detail;

std::shared_ptr<adm::Document> createCommonDoc() {
  auto doc = Document::create();
  adm::addCommonDefinitionsTo(doc);
  return doc;
}

std::shared_ptr<adm::Document> commonDoc() {
  auto static doc = createCommonDoc();
  return doc->deepCopy();
}

TEST_CASE("Common definitions not linked to uids do not convert to layouts") {
  auto doc = commonDoc();
  auto layouts = find_target_layouts(*doc, common_pack_to_atmos_speaker_map);
  REQUIRE(layouts.empty());
}

TEST_CASE("5.1 common definition converts to atmos 5.1") {
  auto doc = commonDoc();
  adm::addSimpleCommonDefinitionsObjectTo(doc, "Test", "0+5+0");
  auto layouts = find_target_layouts(*doc, common_pack_to_atmos_speaker_map);
  REQUIRE(layouts.size() == 1);
  auto const &mappedSpeakers = layouts.begin()->second.target_layout;
  REQUIRE(mappedSpeakers.size() == 6);
}

TEST_CASE("Two distinct layouts -> two distinct layouts") {
  auto doc = commonDoc();
  adm::addSimpleCommonDefinitionsObjectTo(doc, "Test", "0+5+0");
  adm::addSimpleCommonDefinitionsObjectTo(doc, "Test", "0+2+0");
  auto layouts = find_target_layouts(*doc, common_pack_to_atmos_speaker_map);
  REQUIRE(layouts.size() == 2);
}

TEST_CASE("Empty target map returns empty speaker list") {
  auto mapped = mapped_speaker_set({});
  REQUIRE(mapped.empty());
}

namespace {
std::shared_ptr<AudioPackFormat> pack(SimpleCommonDefinitionsObjectHolder const &holder) {
  auto packs = holder.audioObject->getReferences<AudioPackFormat>();
  REQUIRE(packs.size() == 1);
  return packs.front();
}

std::vector<std::shared_ptr<AudioTrackUid>> uids(SimpleCommonDefinitionsObjectHolder const &holder) {
  auto refs = holder.audioObject->getReferences<AudioTrackUid>();
  return {refs.begin(), refs.end()};
}
}  // namespace

TEST_CASE("Speakers de-duplicated by mapped_speaker_set()") {
  auto doc = commonDoc();
  auto five_one = adm::addSimpleCommonDefinitionsObjectTo(doc, "Test", "0+5+0");
  auto stereo = adm::addSimpleCommonDefinitionsObjectTo(doc, "Test", "0+2+0");
  TargetLayoutMap targets(
      {{pack(five_one), {uids(five_one), {Left, Center, Right, LFE, LeftRearSurround, RightRearSurround}}},
       {pack(stereo), {uids(stereo), {Left, Right}}}});
  auto mapped = mapped_speaker_set(targets);
  REQUIRE(mapped.size() == 6);
}
//
// Created by Richard Bailey on 09/08/2022.
//
