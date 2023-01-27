#include "pack_allocation.hpp"

#include <adm/common_definitions.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace eat::render;
using namespace adm;

template <typename T>
std::vector<T> concat(const std::initializer_list<std::vector<T>> &vecs) {
  std::vector<T> out;
  for (const auto &vec : vecs)
    for (const auto &el : vec) out.push_back(el);
  return out;
}

template <typename T>
std::vector<T> first_n(const std::vector<T> &vec, size_t n) {
  return {vec.begin(), vec.begin() + std::min(vec.size(), n)};
}

struct Harness {
  Harness() {
    adm = getCommonDefinitions();

    pack_1_0 = std::make_shared<AllocationPack>();
    pack_1_0->root_pack = get_pack("AP_00010001");
    pack_1_0->channels.push_back({
        get_channel("AC_00010003"),
        {get_pack("AP_00010001")},
    });
    tracks_1_0.push_back(std::make_shared<AllocationTrack>(get_channel("AC_00010003"), get_pack("AP_00010001")));

    pack_2_0 = std::make_shared<AllocationPack>();
    pack_2_0->root_pack = get_pack("AP_00010002");
    pack_2_0->channels.push_back({
        get_channel("AC_00010001"),
        {get_pack("AP_00010002")},
    });
    pack_2_0->channels.push_back({
        get_channel("AC_00010002"),
        {get_pack("AP_00010002")},
    });
    tracks_2_0.push_back(std::make_shared<AllocationTrack>(get_channel("AC_00010001"), get_pack("AP_00010002")));
    tracks_2_0.push_back(std::make_shared<AllocationTrack>(get_channel("AC_00010002"), get_pack("AP_00010002")));

    // dorectspeakers aren't actually structured with nested packs, but it's a
    // bit simpler to test bested packs with fewer channels
    pack_5_0 = std::make_shared<AllocationPack>();
    pack_5_0->root_pack = get_pack("AP_0001000c");
    pack_5_0->channels.push_back({
        get_channel("AC_00010001"),
        {get_pack("AP_0001000c"), get_pack("AP_00010002")},
    });
    pack_5_0->channels.push_back({
        get_channel("AC_00010002"),
        {get_pack("AP_0001000c"), get_pack("AP_00010002")},
    });
    pack_5_0->channels.push_back({
        get_channel("AC_00010003"),
        {get_pack("AP_0001000c")},
    });
    pack_5_0->channels.push_back({
        get_channel("AC_00010005"),
        {get_pack("AP_0001000c")},
    });
    pack_5_0->channels.push_back({
        get_channel("AC_00010006"),
        {get_pack("AP_0001000c")},
    });

    tracks_5_0.push_back(std::make_shared<AllocationTrack>(get_channel("AC_00010001"), get_pack("AP_0001000c")));
    tracks_5_0.push_back(std::make_shared<AllocationTrack>(get_channel("AC_00010002"), get_pack("AP_0001000c")));
    tracks_5_0.push_back(std::make_shared<AllocationTrack>(get_channel("AC_00010003"), get_pack("AP_0001000c")));
    tracks_5_0.push_back(std::make_shared<AllocationTrack>(get_channel("AC_00010005"), get_pack("AP_0001000c")));
    tracks_5_0.push_back(std::make_shared<AllocationTrack>(get_channel("AC_00010006"), get_pack("AP_0001000c")));

    tracks_5_0_ref_2_0.push_back(
        std::make_shared<AllocationTrack>(get_channel("AC_00010001"), get_pack("AP_00010002")));
    tracks_5_0_ref_2_0.push_back(
        std::make_shared<AllocationTrack>(get_channel("AC_00010002"), get_pack("AP_00010002")));
    tracks_5_0_ref_2_0.push_back(
        std::make_shared<AllocationTrack>(get_channel("AC_00010003"), get_pack("AP_0001000c")));
    tracks_5_0_ref_2_0.push_back(
        std::make_shared<AllocationTrack>(get_channel("AC_00010005"), get_pack("AP_0001000c")));
    tracks_5_0_ref_2_0.push_back(
        std::make_shared<AllocationTrack>(get_channel("AC_00010006"), get_pack("AP_0001000c")));
  }

  PackFmtPointer get_pack(const std::string &id) { return adm->lookup(parseAudioPackFormatId(id)); }

  ChannelFmtPointer get_channel(const std::string &id) { return adm->lookup(parseAudioChannelFormatId(id)); }

  DocumentPtr adm;

  std::shared_ptr<AllocationPack> pack_1_0;
  std::vector<Ref<AllocationTrack>> tracks_1_0;

  std::shared_ptr<AllocationPack> pack_2_0;
  std::vector<Ref<AllocationTrack>> tracks_2_0;

  std::shared_ptr<AllocationPack> pack_5_0;
  std::vector<Ref<AllocationTrack>> tracks_5_0;
  std::vector<Ref<AllocationTrack>> tracks_5_0_ref_2_0;
};

TEST_CASE("pack_allocation_empty") {
  Harness h;

  auto alloc = allocate_packs({h.pack_2_0}, {}, std::nullopt, 0, 2);
  REQUIRE(alloc.size() == 1);
  REQUIRE(alloc.at(0).size() == 0);
}

TEST_CASE("pack_allocation_basic_pack_refs") {
  Harness h;

  auto alloc = allocate_packs({h.pack_2_0}, h.tracks_2_0, {{h.pack_2_0->root_pack}}, 0, 2);
  REQUIRE(alloc.size() == 1);
  REQUIRE(alloc.at(0).size() == 1);
  REQUIRE(alloc.at(0).at(0).pack == h.pack_2_0);
  REQUIRE(alloc.at(0).at(0).allocation.size() == 2);
  REQUIRE(alloc.at(0).at(0).allocation.at(0) == h.tracks_2_0.at(0));
  REQUIRE(alloc.at(0).at(0).allocation.at(1) == h.tracks_2_0.at(1));
}

TEST_CASE("pack_allocation_basic_no_pack_refs") {
  Harness h;

  auto alloc = allocate_packs({h.pack_2_0}, h.tracks_2_0, std::nullopt, 0, 2);
  REQUIRE(alloc.size() == 1);
  REQUIRE(alloc.at(0).size() == 1);
  REQUIRE(alloc.at(0).at(0).pack == h.pack_2_0);
  REQUIRE(alloc.at(0).at(0).allocation.size() == 2);
  REQUIRE(alloc.at(0).at(0).allocation.at(0) == h.tracks_2_0.at(0));
  REQUIRE(alloc.at(0).at(0).allocation.at(1) == h.tracks_2_0.at(1));
}

TEST_CASE("pack_allocation_basic_first_silent") {
  Harness h;

  auto alloc = allocate_packs({h.pack_2_0}, {h.tracks_2_0[1]}, {{h.pack_2_0->root_pack}}, 1, 2);
  REQUIRE(alloc.size() == 1);
  REQUIRE(alloc.at(0).size() == 1);
  REQUIRE(alloc.at(0).at(0).pack == h.pack_2_0);
  REQUIRE(alloc.at(0).at(0).allocation.size() == 2);
  REQUIRE(!alloc.at(0).at(0).allocation.at(0));
  REQUIRE(alloc.at(0).at(0).allocation.at(1) == h.tracks_2_0.at(1));
}

TEST_CASE("pack_allocation_basic_second_silent") {
  Harness h;

  auto alloc = allocate_packs({h.pack_2_0}, {h.tracks_2_0[0]}, {{h.pack_2_0->root_pack}}, 1, 2);
  REQUIRE(alloc.size() == 1);
  REQUIRE(alloc.at(0).size() == 1);
  REQUIRE(alloc.at(0).at(0).pack == h.pack_2_0);
  REQUIRE(alloc.at(0).at(0).allocation.size() == 2);
  REQUIRE(alloc.at(0).at(0).allocation.at(0) == h.tracks_2_0.at(0));
  REQUIRE(!alloc.at(0).at(0).allocation.at(1));
}

TEST_CASE("pack_allocation_basic_both_silent") {
  Harness h;

  auto alloc = allocate_packs({h.pack_2_0}, {}, {{h.pack_2_0->root_pack}}, 2, 2);
  REQUIRE(alloc.size() == 1);
  REQUIRE(alloc.at(0).size() == 1);
  REQUIRE(alloc.at(0).at(0).pack == h.pack_2_0);
  REQUIRE(alloc.at(0).at(0).allocation.size() == 2);
  REQUIRE(!alloc.at(0).at(0).allocation.at(0));
  REQUIRE(!alloc.at(0).at(0).allocation.at(1));
}

TEST_CASE("pack_allocation_basic_not_enough_channels") {
  Harness h;

  auto alloc = allocate_packs({h.pack_2_0}, {h.tracks_2_0[0]}, {{h.pack_2_0->root_pack}}, 0, 2);
  REQUIRE(alloc.size() == 0);
}

TEST_CASE("pack_allocation_basic_not_enough_channels_silent") {
  Harness h;

  auto alloc = allocate_packs({h.pack_2_0}, {}, {{h.pack_2_0->root_pack}}, 1, 2);
  REQUIRE(alloc.size() == 0);
}

TEST_CASE("pack_allocation_both_stereo_pack_ref") {
  Harness h;

  auto alloc = allocate_packs({h.pack_2_0, h.pack_5_0}, h.tracks_2_0, {{h.pack_2_0->root_pack}}, 0, 2);
  REQUIRE(alloc.size() == 1);
  REQUIRE(alloc.at(0).size() == 1);
  REQUIRE(alloc.at(0).at(0).pack == h.pack_2_0);
  REQUIRE(alloc.at(0).at(0).allocation.size() == 2);
  REQUIRE(alloc.at(0).at(0).allocation.at(0) == h.tracks_2_0.at(0));
  REQUIRE(alloc.at(0).at(0).allocation.at(1) == h.tracks_2_0.at(1));
}

TEST_CASE("pack_allocation_both_51_pack_ref") {
  Harness h;

  auto alloc = allocate_packs({h.pack_2_0, h.pack_5_0}, h.tracks_5_0, {{h.pack_5_0->root_pack}}, 0, 2);
  REQUIRE(alloc.size() == 1);
  REQUIRE(alloc.at(0).size() == 1);
  REQUIRE(alloc.at(0).at(0).pack == h.pack_5_0);
  REQUIRE(alloc.at(0).at(0).allocation.size() == 5);
  for (size_t c = 0; c < 5; c++) REQUIRE(alloc.at(0).at(0).allocation.at(c) == h.tracks_5_0.at(c));
}

TEST_CASE("pack_allocation_both_51_no_pack_ref") {
  Harness h;

  auto alloc = allocate_packs({h.pack_2_0, h.pack_5_0}, h.tracks_5_0, std::nullopt, 0, 2);
  REQUIRE(alloc.size() == 1);
  REQUIRE(alloc.at(0).size() == 1);
  REQUIRE(alloc.at(0).at(0).pack == h.pack_5_0);
  REQUIRE(alloc.at(0).at(0).allocation.size() == 5);
  for (size_t c = 0; c < 5; c++) REQUIRE(alloc.at(0).at(0).allocation.at(c) == h.tracks_5_0.at(c));
}

TEST_CASE("pack_allocation_both_51_ref_20_no_pack_ref") {
  Harness h;

  auto alloc = allocate_packs({h.pack_2_0, h.pack_5_0}, h.tracks_5_0_ref_2_0, std::nullopt, 0, 2);
  REQUIRE(alloc.size() == 1);
  REQUIRE(alloc.at(0).size() == 1);
  REQUIRE(alloc.at(0).at(0).pack == h.pack_5_0);
  REQUIRE(alloc.at(0).at(0).allocation.size() == 5);
  for (size_t c = 0; c < 5; c++) REQUIRE(alloc.at(0).at(0).allocation.at(c) == h.tracks_5_0_ref_2_0.at(c));
}

TEST_CASE("pack_allocation_both_both_no_pack_ref") {
  Harness h;

  auto alloc = allocate_packs({h.pack_2_0, h.pack_5_0}, concat({h.tracks_2_0, h.tracks_5_0}), std::nullopt, 0, 2);
  REQUIRE(alloc.size() == 1);
  REQUIRE(alloc.at(0).size() == 2);
  REQUIRE(alloc.at(0).at(0).pack == h.pack_2_0);
  REQUIRE(alloc.at(0).at(0).allocation.size() == 2);
  for (size_t c = 0; c < 2; c++) REQUIRE(alloc.at(0).at(0).allocation.at(c) == h.tracks_2_0.at(c));
  REQUIRE(alloc.at(0).at(1).pack == h.pack_5_0);
  REQUIRE(alloc.at(0).at(1).allocation.size() == 5);
  for (size_t c = 0; c < 5; c++) REQUIRE(alloc.at(0).at(1).allocation.at(c) == h.tracks_5_0.at(c));
}

TEST_CASE("pack_allocation_both_both_no_pack_ref_ambiguous") {
  Harness h;

  auto alloc =
      allocate_packs({h.pack_2_0, h.pack_5_0}, concat({h.tracks_2_0, h.tracks_5_0_ref_2_0}), std::nullopt, 0, 2);
  REQUIRE(alloc.size() == 2);
}

TEST_CASE("pack_allocation_both_both_silent") {
  Harness h;

  auto alloc = allocate_packs({h.pack_2_0, h.pack_5_0}, first_n(h.tracks_5_0, 4), std::nullopt, 3, 2);
  REQUIRE(alloc.size() == 1);
  REQUIRE(alloc.at(0).at(0).pack == h.pack_5_0);
  REQUIRE(alloc.at(0).at(0).allocation.size() == 5);
  for (size_t c = 0; c < 4; c++) REQUIRE(alloc.at(0).at(0).allocation.at(c) == h.tracks_5_0.at(c));
  REQUIRE(!alloc.at(0).at(0).allocation.at(4));
  REQUIRE(alloc.at(0).size() == 2);
  REQUIRE(alloc.at(0).at(1).pack == h.pack_2_0);
  REQUIRE(alloc.at(0).at(1).allocation.size() == 2);
  for (size_t c = 0; c < 2; c++) REQUIRE(!alloc.at(0).at(1).allocation.at(c));
}

TEST_CASE("pack_allocation_doublesilent") {
  Harness h;

  auto alloc = allocate_packs({h.pack_5_0, h.pack_2_0}, {}, {{h.pack_5_0->root_pack, h.pack_2_0->root_pack}}, 7, 2);
  REQUIRE(alloc.size() == 1);
  REQUIRE(alloc.at(0).at(0).pack == h.pack_5_0);
  REQUIRE(alloc.at(0).at(0).allocation.size() == 5);
  for (size_t c = 0; c < 5; c++) REQUIRE(!alloc.at(0).at(0).allocation.at(c));
  REQUIRE(!alloc.at(0).at(0).allocation.at(4));
  REQUIRE(alloc.at(0).size() == 2);
  REQUIRE(alloc.at(0).at(1).pack == h.pack_2_0);
  REQUIRE(alloc.at(0).at(1).allocation.size() == 2);
  for (size_t c = 0; c < 2; c++) REQUIRE(!alloc.at(0).at(1).allocation.at(c));
}

TEST_CASE("pack_allocation_multiple_mono_refs") {
  Harness h;

  auto alloc = allocate_packs({h.pack_5_0, h.pack_2_0, h.pack_1_0}, concat({h.tracks_1_0, h.tracks_1_0}),
                              {{h.pack_1_0->root_pack, h.pack_1_0->root_pack}}, 0, 2);
  REQUIRE(alloc.size() == 1);
}

TEST_CASE("pack_allocation_multiple_mono_norefs") {
  Harness h;

  auto alloc =
      allocate_packs({h.pack_5_0, h.pack_2_0, h.pack_1_0}, concat({h.tracks_1_0, h.tracks_1_0}), std::nullopt, 0, 2);
  REQUIRE(alloc.size() == 1);
}

TEST_CASE("pack_allocation_multiple_mono_norefs_silent") {
  Harness h;

  auto alloc = allocate_packs({h.pack_5_0, h.pack_2_0, h.pack_1_0}, {}, std::nullopt, 2, 2);
  REQUIRE(alloc.size() == 1);
}

TEST_CASE("pack_allocation_multiple_stereo_refs") {
  Harness h;

  auto alloc = allocate_packs({h.pack_5_0, h.pack_2_0, h.pack_1_0}, concat({h.tracks_2_0, h.tracks_2_0}),
                              {{h.pack_2_0->root_pack, h.pack_2_0->root_pack}}, 0, 2);
  REQUIRE(alloc.size() == 2);
}

TEST_CASE("pack_allocation_multiple_stereo_norefs") {
  Harness h;

  auto alloc =
      allocate_packs({h.pack_5_0, h.pack_2_0, h.pack_1_0}, concat({h.tracks_2_0, h.tracks_2_0}), std::nullopt, 0, 2);
  REQUIRE(alloc.size() == 2);
}

TEST_CASE("pack_allocation_multiple_stereo_norefs_silent") {
  Harness h;

  auto alloc = allocate_packs({h.pack_5_0, h.pack_2_0, h.pack_1_0}, {}, std::nullopt, 4, 2);
  REQUIRE(alloc.size() == 1);
}
