//
// Created by Richard Bailey on 27/09/2022.
//

#include "eat/process/block_resampling.hpp"

#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>

#include "../utilities/ostream_operators.hpp"

using namespace eat::process;
using namespace adm;
using namespace std::chrono_literals;
TEST_CASE("Resampling single block with rtime and duration equal to minimum returns block with same timing") {
  std::vector<AudioBlockFormatObjects> blocks{AudioBlockFormatObjects{
      SphericalPosition{Azimuth{0.5f}, Elevation{0.5f}, Distance{0.5f}}, Rtime{0ms}, Duration{500ms}}};
  auto blocksRange = BlockFormatsRange<AudioBlockFormatObjects>{blocks};
  auto resampled = resample_to_minimum_preserving_zero(blocksRange, 500ms);
  REQUIRE(resampled.size() == 1);
  SECTION("Correct timing") {
    CHECK(resampled.front().get<adm::Rtime>() == 0ms);
    CHECK(resampled.front().get<adm::Duration>() == 500ms);
  }
  SECTION("Parameter values") {
    auto const &block = resampled.front();
    CHECK(block.has<adm::SphericalPosition>());
    auto const &position = block.get<adm::SphericalPosition>();
    CHECK(position.get<adm::Elevation>().get() == 0.5f);
    CHECK(position.get<adm::Azimuth>().get() == 0.5f);
    CHECK(position.get<adm::Distance>().get() == 0.5f);
  }
}

TEST_CASE("Resampling single block with rtime and duration larger than minimum returns block with same timing") {
  auto const P_VALUE = 0.6f;
  std::vector<AudioBlockFormatObjects> blocks{
      AudioBlockFormatObjects{CartesianPosition{X{P_VALUE}, Y{P_VALUE}, Z{P_VALUE}}, Rtime{0ms}, Duration{1000ms}}};
  auto blocksRange = BlockFormatsRange<AudioBlockFormatObjects>{blocks};
  auto resampled = resample_to_minimum_preserving_zero(blocksRange, 500ms);
  REQUIRE(resampled.size() == 1);
  SECTION("Correct timing") {
    CHECK(resampled.front().get<adm::Rtime>() == 0ms);
    CHECK(resampled.front().get<adm::Duration>() == 1000ms);
  }
  SECTION("Parameter values") {
    auto const &first = resampled.front();
    REQUIRE(first.has<CartesianPosition>());
    auto const pos = first.get<adm::CartesianPosition>();
    CHECK(pos.get<X>().get() == P_VALUE);
    CHECK(pos.get<Y>().get() == P_VALUE);
    CHECK(pos.get<Z>().get() == P_VALUE);
  }
}

namespace {
template <typename DurationT, std::size_t Sz>
std::vector<adm::AudioBlockFormatObjects> create_block_train(std::array<DurationT, Sz> durations,
                                                             std::array<float, Sz> values) {
  std::vector<adm::AudioBlockFormatObjects> blocks;
  blocks.reserve(Sz);
  auto rtime = DurationT::zero();
  for (std::size_t i = 0; i != Sz; ++i) {
    blocks.emplace_back(CartesianPosition{X{values[i]}, Y{values[i]}, Z{values[i]}}, Rtime(rtime),
                        Duration{durations[i]});
    rtime += durations[i];
  }
  return blocks;
}

template <typename DurationT, std::size_t Sz>
std::vector<adm::AudioBlockFormatObjects> create_block_train(std::array<DurationT, Sz> rtimes,
                                                             std::array<DurationT, Sz> durations,
                                                             std::array<float, Sz> values) {
  std::vector<adm::AudioBlockFormatObjects> blocks;
  blocks.reserve(Sz);
  for (std::size_t i = 0; i != Sz; ++i) {
    blocks.emplace_back(CartesianPosition{X{values[i]}, Y{values[i]}}, Rtime(rtimes[i]), Duration{durations[i]});
  }
  return blocks;
}
}  // namespace

TEST_CASE(
    "Resampling two blocks with first zero length and second with rtime and duration equal to minimum returns blocks "
    "with same timing") {
  auto blocks = create_block_train(std::array{0ms, 500ms}, std::array{1.0f, 0.0f});
  auto resampled = resample_to_minimum_preserving_zero(blocks, 500ms);
  REQUIRE(resampled.size() == 2);
  CHECK(resampled[0].get<adm::Rtime>() == 0ms);
  CHECK(resampled[0].get<adm::Duration>() == 0ms);
  CHECK(resampled[1].get<adm::Rtime>() == 0ms);
  CHECK(resampled[1].get<adm::Duration>() == 500ms);
}

TEST_CASE("De-duplicate zero length blocks") {
  auto rtimes = std::array{0ms, 0ms, 0ms};
  auto durations = std::array{0ms, 0ms, 0ms};
  auto blocks = create_block_train(rtimes, durations, {0.f, 1.f, 2.f});
  auto de_duped = de_duplicate_zero_length_blocks(blocks);
  REQUIRE(de_duped.size() == 1);
  REQUIRE(de_duped.front().get<adm::CartesianPosition>().get<adm::X>() == 2.f);
}

std::tuple<adm::X, adm::Y, adm::Z> position_components(adm::CartesianPosition const &pos) {
  auto x = pos.get<adm::X>();
  auto y = pos.get<adm::Y>();
  auto z = pos.get<adm::Z>();
  return {x, y, z};
}

TEST_CASE("Multi-parameter interpolation") {
  auto const FIRST_INPUT = .25f;
  auto const SECOND_INPUT = 1.f;
  auto const FIRST_EXPECTED = .5f;
  auto const SECOND_EXPECTED = 1.f;
  auto blocks = create_block_train(std::array{10ms, 30ms}, std::array{0.25f, 1.0f});
  blocks[0].set(adm::Gain(FIRST_INPUT));
  blocks[1].set(adm::Gain(SECOND_INPUT));
  blocks[0].set(adm::Width(FIRST_INPUT));
  blocks[1].set(adm::Width(SECOND_INPUT));
  blocks[0].set(adm::Height(FIRST_INPUT));
  blocks[1].set(adm::Height(SECOND_INPUT));
  blocks[0].set(adm::Depth(FIRST_INPUT));
  blocks[1].set(adm::Depth(SECOND_INPUT));
  blocks[0].set(adm::Diffuse(FIRST_INPUT));
  blocks[1].set(adm::Diffuse(SECOND_INPUT));
  // NB shouldn't have both position and azimuth ranges on an ObjectDivergence
  blocks[0].set(adm::ObjectDivergence{adm::Divergence{FIRST_INPUT}, adm::PositionRange{FIRST_INPUT},
                                      adm::AzimuthRange{FIRST_INPUT}});
  blocks[1].set(adm::ObjectDivergence{adm::Divergence{SECOND_INPUT}, adm::PositionRange{SECOND_INPUT},
                                      adm::AzimuthRange{SECOND_INPUT}});

  auto resampled = resample_to_minimum_preserving_zero(blocks, 20ms);
  REQUIRE(resampled.size() == 2);
  SECTION("Gain") {
    REQUIRE(resampled[0].has<adm::Gain>());
    REQUIRE(resampled[1].has<adm::Gain>());
    CHECK(resampled[0].get<adm::Gain>().get() == Catch::Approx(FIRST_EXPECTED));
    CHECK(resampled[1].get<adm::Gain>().get() == Catch::Approx(SECOND_EXPECTED));
  }
  SECTION("Width") {
    REQUIRE(resampled[0].has<adm::Width>());
    REQUIRE(resampled[1].has<adm::Width>());
    CHECK(resampled[0].get<adm::Width>().get() == Catch::Approx(FIRST_EXPECTED));
    CHECK(resampled[1].get<adm::Width>().get() == Catch::Approx(SECOND_EXPECTED));
  }
  SECTION("Height") {
    REQUIRE(resampled[0].has<adm::Height>());
    REQUIRE(resampled[1].has<adm::Height>());
    CHECK(resampled[0].get<adm::Height>().get() == Catch::Approx(FIRST_EXPECTED));
    CHECK(resampled[1].get<adm::Height>().get() == Catch::Approx(SECOND_EXPECTED));
  }
  SECTION("Depth") {
    REQUIRE(resampled[0].has<adm::Depth>());
    REQUIRE(resampled[1].has<adm::Depth>());
    CHECK(resampled[0].get<adm::Depth>().get() == Catch::Approx(FIRST_EXPECTED));
    CHECK(resampled[1].get<adm::Depth>().get() == Catch::Approx(SECOND_EXPECTED));
  }
  SECTION("Diffuse") {
    REQUIRE(resampled[0].has<adm::Diffuse>());
    REQUIRE(resampled[1].has<adm::Diffuse>());
    CHECK(resampled[0].get<adm::Diffuse>().get() == Catch::Approx(FIRST_EXPECTED));
    CHECK(resampled[1].get<adm::Diffuse>().get() == Catch::Approx(SECOND_EXPECTED));
  }
  SECTION("ObjectDivergence") {
    // Note AzimuthRange and PositionRange are mutially exclusive so maybe this test will break
    // if libadm gets strict.
    REQUIRE(resampled[0].has<adm::ObjectDivergence>());
    REQUIRE(resampled[1].has<adm::ObjectDivergence>());
    auto first_divergence = resampled[0].get<adm::ObjectDivergence>();
    CHECK(first_divergence.get<adm::Divergence>().get() == Catch::Approx(FIRST_EXPECTED));
    CHECK(first_divergence.get<adm::AzimuthRange>().get() == Catch::Approx(FIRST_EXPECTED));
    CHECK(first_divergence.get<adm::PositionRange>().get() == Catch::Approx(FIRST_EXPECTED));
    auto second_divergence = resampled[1].get<adm::ObjectDivergence>();
    CHECK(second_divergence.get<adm::Divergence>().get() == Catch::Approx(SECOND_EXPECTED));
    CHECK(second_divergence.get<adm::AzimuthRange>().get() == Catch::Approx(SECOND_EXPECTED));
    CHECK(second_divergence.get<adm::PositionRange>().get() == Catch::Approx(SECOND_EXPECTED));
  }
}

TEST_CASE("Short + long blocks -> two min length") {
  auto blocks = create_block_train(std::array{10ms, 30ms}, std::array{0.25f, 1.0f});
  auto restricted = resample_to_minimum_preserving_zero(blocks, 20ms);
  REQUIRE(restricted.size() == 2);
  SECTION("Correct timing") {
    CHECK(restricted[0].get<adm::Rtime>() == 0ms);
    CHECK(restricted[1].get<adm::Rtime>() == 20ms);
    CHECK(restricted[0].get<adm::Duration>() == 20ms);
    CHECK(restricted[1].get<adm::Duration>() == 20ms);
  }
  SECTION("Parameter values") {
    REQUIRE(restricted[0].has<adm::CartesianPosition>());
    auto const first_position = restricted[0].get<adm::CartesianPosition>();
    auto [x1, y1, z1] = position_components(first_position);
    CHECK(x1.get() == 0.5f);
    CHECK(y1.get() == 0.5f);
    CHECK(z1.get() == 0.5f);

    REQUIRE(restricted[1].has<adm::CartesianPosition>());
    auto const second_position = restricted[1].get<adm::CartesianPosition>();
    auto [x2, y2, z2] = position_components(second_position);
    CHECK(x2.get() == 1.f);
    CHECK(y2.get() == 1.f);
    CHECK(z2.get() == 1.f);
  }
}

TEST_CASE("Restrict to minimum 20ms without leftovers") {
  auto blocks = create_block_train(std::array{0ms, 30ms, 10ms, 20ms},
                                   std::array{0.f, .5f, 0.5f + 1.0f / 6.0f, 1.f});  // linear ramp
  auto restricted = resample_to_minimum_preserving_zero(blocks, 20ms);
  REQUIRE(restricted.size() == 4);
  SECTION("check correct timing") {
    CHECK(restricted[0].get<adm::Rtime>().get().asNanoseconds() == 0ms);
    CHECK(restricted[1].get<adm::Rtime>().get().asNanoseconds() == 0ms);
    CHECK(restricted[2].get<adm::Rtime>().get().asNanoseconds() == 20ms);
    CHECK(restricted[3].get<adm::Rtime>().get().asNanoseconds() == 40ms);

    CHECK(restricted[0].get<adm::Duration>().get().asNanoseconds() == 0ms);
    CHECK(restricted[1].get<adm::Duration>().get().asNanoseconds() == 20ms);
    CHECK(restricted[2].get<adm::Duration>().get().asNanoseconds() == 20ms);
    CHECK(restricted[3].get<adm::Duration>().get().asNanoseconds() == 20ms);
  }
  SECTION("Check correct values") {
    auto values = std::array{0.f, 1.0f / 3, 1 - 1.0f / 3, 1.0f};
    for (std::size_t i = 0; i != 4; ++i) {
      using Catch::Approx;
      REQUIRE(restricted[i].has<adm::CartesianPosition>());
      auto position = restricted[i].get<adm::CartesianPosition>();
      INFO("i == " + std::to_string(i));
      CHECK(position.get<adm::X>().get() == Approx(values[i]));
      CHECK(position.get<adm::Y>().get() == Approx(values[i]));
      CHECK(position.get<adm::Z>().get() == Approx(values[i]));
    }
  }
}

TEST_CASE("Restrict to minimum 20ms with leftover block at start") {
  // blocks[1] and blocks[2] should be combined so resampled[1] is > minimum
  // end times: 0ms, 10ms,   30ms, 60ms
  auto blocks =
      create_block_train(std::array{0ms, 10ms, 20ms, 30ms}, std::array{0.f, 1.5f / 6, 0.5f, 1.f});  // linear ramp
  auto restricted = resample_to_minimum_preserving_zero(blocks, 20ms);
  REQUIRE(restricted.size() == 3);
  SECTION("check correct timing") {
    CHECK(restricted[0].get<adm::Rtime>().get().asNanoseconds() == 0ms);
    CHECK(restricted[1].get<adm::Rtime>().get().asNanoseconds() == 0ms);
    CHECK(restricted[2].get<adm::Rtime>().get().asNanoseconds() == 30ms);
    CHECK(restricted[0].get<adm::Duration>().get().asNanoseconds() == 0ms);
    CHECK(restricted[1].get<adm::Duration>().get().asNanoseconds() == 30ms);
    CHECK(restricted[2].get<adm::Duration>().get().asNanoseconds() == 30ms);
  }
  SECTION("Check correct values") {
    auto values = std::array{0.f, .5f, 1.f};
    for (std::size_t i = 0; i != 3; ++i) {
      REQUIRE(restricted[i].has<adm::CartesianPosition>());
      using Catch::Approx;
      auto position = restricted[i].get<adm::CartesianPosition>();
      INFO("i == " + std::to_string(i));
      CHECK(position.get<adm::X>().get() == Approx(values[i]));
      CHECK(position.get<adm::Y>().get() == Approx(values[i]));
      CHECK(position.get<adm::Z>().get() == Approx(values[i]));
    }
  }
}

template <std::size_t Sz>
std::vector<adm::AudioBlockFormatObjects> create_block_train_fractional(std::array<int, Sz> durations,
                                                                        std::array<float, Sz> values,
                                                                        int rate = 44100) {
  std::vector<adm::AudioBlockFormatObjects> blocks;
  blocks.reserve(Sz);
  auto rtime = 0;
  for (std::size_t i = 0; i != Sz; ++i) {
    blocks.emplace_back(CartesianPosition{X{values[i]}, Y{values[i]}, Z{values[i]}},
                        Rtime(Time(FractionalTime(rtime, rate))), Duration{Time(FractionalTime(durations[i], rate))});
    rtime += durations[i];
  }
  return blocks;
}

TEST_CASE("Restrict to minimum 20 samples at 44.1k") {
  auto const S_RATE = 44100;
  auto blocks = create_block_train_fractional(std::array{0, 10, 20, 30}, std::array{0.f, 1.5f / 6, 0.5f, 1.f},
                                              S_RATE);  // linear ramp
  auto restricted = resample_to_minimum_preserving_zero(blocks, adm::FractionalTime(20, S_RATE));
  REQUIRE(restricted.size() == 3);
  SECTION("check correct timing") {
    CHECK(restricted[0].get<adm::Rtime>().get().asFractional().numerator() == 0);
    CHECK(restricted[1].get<adm::Rtime>().get().asFractional().numerator() == 0);
    CHECK(restricted[2].get<adm::Rtime>().get().asFractional().numerator() == 30);
    CHECK(restricted[0].get<adm::Duration>().get().asFractional().numerator() == 0);
    CHECK(restricted[1].get<adm::Duration>().get().asFractional().numerator() == 30);
    CHECK(restricted[2].get<adm::Duration>().get().asFractional().numerator() == 30);
    for (std::size_t i = 0; i != restricted.size(); ++i) {
      INFO(i);
      CHECK(restricted[i].get<adm::Rtime>()->asFractional().denominator() == S_RATE);
      CHECK(restricted[i].get<adm::Duration>()->asFractional().denominator() == S_RATE);
    }
  }
  SECTION("Check correct values") {
    auto values = std::array{0.f, .5f, 1.f};
    for (std::size_t i = 0; i != 3; ++i) {
      REQUIRE(restricted[i].has<adm::CartesianPosition>());
      using Catch::Approx;
      auto position = restricted[i].get<adm::CartesianPosition>();
      INFO("i == " + std::to_string(i));
      CHECK(position.get<adm::X>().get() == Approx(values[i]));
      CHECK(position.get<adm::Y>().get() == Approx(values[i]));
      CHECK(position.get<adm::Z>().get() == Approx(values[i]));
    }
  }
}
