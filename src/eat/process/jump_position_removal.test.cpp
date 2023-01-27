//
// Created by Richard Bailey on 08/09/2022.
//
#include "eat/process/jump_position_removal.hpp"

#include <adm/utilities/object_creation.hpp>
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <tuple>

#include "../utilities/ostream_operators.hpp"
#include "eat/process/adm_time_extras.hpp"
using namespace eat::process;
using namespace std::chrono_literals;

namespace adm {
bool operator==(CartesianPosition const &lhs, CartesianPosition const &rhs) {
  return std::tie(lhs.get<X>().get(), lhs.get<Y>().get(), lhs.get<Z>().get()) ==
         std::tie(rhs.get<X>().get(), rhs.get<Y>().get(), rhs.get<Z>().get());
}

}  // namespace adm

using namespace adm;
using namespace eat::admx;

TEST_CASE("Empty range returns empty range") {
  auto holder = adm::createSimpleObject("Test");
  auto blocks = remove_jump_position(holder.audioChannelFormat->getElements<adm::AudioBlockFormatObjects>());
  REQUIRE(blocks.size() == 0);
}

namespace {
void do_common_checks(std::vector<AudioBlockFormatObjects> const &blocks) {
  SECTION("Blocks are contiguous") {
    auto it = std::adjacent_find(blocks.begin(), blocks.end(),
                                 [](AudioBlockFormatObjects const &lhs, AudioBlockFormatObjects const &rhs) {
                                   return lhs.get<Rtime>()->asNanoseconds() + lhs.get<Duration>()->asNanoseconds() !=
                                          rhs.get<Rtime>()->asNanoseconds();
                                 });
    INFO("Block number before discontinuity: " + std::to_string(std::distance(blocks.begin(), it)));
    REQUIRE(it == blocks.end());
  }

  SECTION("Blocks have cleared Ids") {
    REQUIRE(std::all_of(blocks.begin(), blocks.end(), [](AudioBlockFormatObjects const &object) {
      return object.get<AudioBlockFormatId>() == AudioBlockFormatId{};
    }));
  }

  SECTION("Blocks do not have jump position flag set") {
    for (std::size_t i = 0; i != blocks.size(); ++i) {
      auto const &block = blocks.at(i);
      INFO("block " + std::to_string(i));
      CHECK(!block.get<adm::JumpPosition>().get<adm::JumpPositionFlag>().get());
    }
  }
}
}  // namespace

TEST_CASE("Single jump position block format results in single block format") {
  auto holder = adm::createSimpleObject("Test");
  holder.audioChannelFormat->add(adm::AudioBlockFormatObjects(adm::CartesianPosition{}, adm::JumpPosition{}));
  auto blocks = remove_jump_position(holder.audioChannelFormat->getElements<adm::AudioBlockFormatObjects>());
  REQUIRE(blocks.size() == 1);
  do_common_checks(blocks);
  auto const &block = blocks.front();
  SECTION("With default jump position") { CHECK(block.isDefault<adm::JumpPosition>()); }
  SECTION("With zero RTime") {
    REQUIRE(block.has<adm::Rtime>());
    CHECK(block.get<adm::Rtime>() == 0ns);
  }
  SECTION("And no duration") { CHECK((!block.has<adm::Duration>())); }
}

TEST_CASE("Zero length block + Jump position block with non-zero interpolation length. Times in nanoseconds") {
  auto interpolationLength = 10ms;
  auto startBlock = AudioBlockFormatObjects(CartesianPosition{X{0.}, Y{0.}, Z{0.}}, Rtime{0ns}, Duration{0ns});
  auto jumpBlock =
      AudioBlockFormatObjects(CartesianPosition{X{1.}, Y{1.}, Z{0.}}, Rtime{0ns}, Duration{20ms},
                              JumpPosition{JumpPositionFlag{true}, InterpolationLength{interpolationLength}});
  auto holder = createSimpleObject("Test");
  holder.audioChannelFormat->add(startBlock);
  holder.audioChannelFormat->add(jumpBlock);

  auto blocks = remove_jump_position(holder.audioChannelFormat->getElements<AudioBlockFormatObjects>());

  SECTION("Converted to 3 blocks") {
    REQUIRE(blocks.size() == 3);

    do_common_checks(blocks);

    SECTION("First block unchanged") {
      auto const &first = blocks.front();
      CHECK(first.get<Rtime>() == startBlock.get<Rtime>());
      CHECK(first.get<Duration>() == startBlock.get<Duration>());
    }

    SECTION("Second block") {
      auto const &secondBlock = blocks.at(1);

      SECTION("is at original rtime") { REQUIRE(secondBlock.get<adm::Rtime>()->asNanoseconds() == 0ns); }

      SECTION("has duration equal to interpolationLength") {
        REQUIRE(secondBlock.get<adm::Duration>()->asNanoseconds() == interpolationLength);
      }
    }
  }
}

TEST_CASE("Zero length block plus jump position block with interpolation length equal to block length") {
  auto interpolationLength = 20ms;
  auto startPosition = CartesianPosition{X{0.5}, Y{0.5}, Z{0.5}};
  auto finalPosition = CartesianPosition{X{1.0}, Y{1.0}, Z{0.0}};
  auto startTime = 0ns;
  auto duration = 20ms;
  auto startBlock = AudioBlockFormatObjects(startPosition, Rtime{startTime}, Duration{0ns});
  auto jumpBlock =
      AudioBlockFormatObjects(finalPosition, Rtime{startTime}, Duration{duration},
                              JumpPosition{JumpPositionFlag{true}, InterpolationLength{interpolationLength}});
  auto holder = createSimpleObject("Test");
  holder.audioChannelFormat->add(startBlock);
  holder.audioChannelFormat->add(jumpBlock);

  auto blocks = remove_jump_position(holder.audioChannelFormat->getElements<AudioBlockFormatObjects>());

  SECTION("Converted to 2 blocks") {
    REQUIRE(blocks.size() == 2);
    do_common_checks(blocks);
  }

  SECTION("First has same values and timing as original first block") {
    auto const &first = blocks.front();
    REQUIRE(first.get<Rtime>()->asNanoseconds() == startTime);
    REQUIRE(first.get<Duration>()->asNanoseconds() == 0ns);
    REQUIRE(first.get<CartesianPosition>() == startPosition);
  }
  SECTION("Second has same values and timing as original second block") {
    auto const &second = blocks.back();
    REQUIRE(second.get<Rtime>()->asNanoseconds() == startTime);
    REQUIRE(second.get<Duration>()->asNanoseconds() == duration);
    REQUIRE(second.get<CartesianPosition>() == finalPosition);
  }
}

TEST_CASE("Adding fractions") {
  auto result = plus(FractionalTime(2, 3), FractionalTime(1, 4));
  REQUIRE(result.numerator() == 11);
  REQUIRE(result.denominator() == 12);
  result = plus(result, FractionalTime(1, 3));
  REQUIRE(result.numerator() == 5);
  REQUIRE(result.denominator() == 4);
}

TEST_CASE("Subtracting fractions") {
  using namespace adm;
  auto result = minus(FractionalTime(11, 12), FractionalTime(1, 4));
  REQUIRE(result.numerator() == 2);
  REQUIRE(result.denominator() == 3);
}

TEST_CASE("Rounding to fractions") {
  SECTION("down") {
    {
      auto result = roundToFractional(0.24s, 2);
      REQUIRE(result.numerator() == 0);
      REQUIRE(result.denominator() == 2);
    }

    {
      auto result = roundToFractional(-0.26s, 2);
      REQUIRE(result.numerator() == -1);
      REQUIRE(result.denominator() == 2);
    }
  }
  SECTION("up") {
    {
      auto result = roundToFractional(0.25s, 2);
      REQUIRE(result.numerator() == 1);
      REQUIRE(result.denominator() == 2);
    }
    {
      auto result = roundToFractional(0.4s, 2);
      REQUIRE(result.numerator() == 1);
      REQUIRE(result.denominator() == 2);
    }
    {
      auto result = roundToFractional(-0.1s, 2);
      REQUIRE(result.numerator() == 0);
      REQUIRE(result.denominator() == 2);
    }
  }
  SECTION("exact") {
    auto result = roundToFractional(0.5s, 2);
    REQUIRE(result.numerator() == 1);
    REQUIRE(result.denominator() == 2);
  }
}
