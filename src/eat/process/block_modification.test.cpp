//
// Created by Richard Bailey on 27/10/2022.
//
#include "eat/process/block_modification.hpp"

#include <adm/elements.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace adm;
using namespace Catch;
using namespace std::chrono_literals;

TEST_CASE("Split at block boundaries") {
  auto const START_VALUE = .0f;
  auto const END_VALUE = .1f;
  AudioBlockFormatObjects prior{CartesianPosition{X{START_VALUE}, Y{START_VALUE}, Z{START_VALUE}}, Rtime{0ns},
                                Duration{0ns}};
  AudioBlockFormatObjects toSplit{CartesianPosition{X{END_VALUE}, Y{END_VALUE}, Z{END_VALUE}}, Rtime(0ns),
                                  Duration(100ns)};

  SECTION("start") {
    auto [first, second] = eat::process::split(prior, toSplit, Rtime(0ns));
    CHECK(first.get<Rtime>()->asNanoseconds() == 0ns);
    CHECK(first.get<Duration>()->asNanoseconds() == 0ns);
    CHECK(second.get<Rtime>()->asNanoseconds() == 0ns);
    CHECK(second.get<Duration>()->asNanoseconds() == 100ns);
    REQUIRE(first.has<CartesianPosition>());
    auto first_position = first.get<CartesianPosition>();
    CHECK(first_position.get<X>() == START_VALUE);
    CHECK(first_position.get<Y>() == START_VALUE);
    CHECK(first_position.get<Z>() == START_VALUE);
    REQUIRE(second.has<CartesianPosition>());
    auto second_position = second.get<CartesianPosition>();
    CHECK(second_position.get<X>() == END_VALUE);
    CHECK(second_position.get<Y>() == END_VALUE);
    CHECK(second_position.get<Z>() == END_VALUE);
  }
  SECTION("end") {
    auto [first, second] = eat::process::split(prior, toSplit, Rtime(100ns));
    CHECK(first.get<Rtime>()->asNanoseconds() == 0ns);
    CHECK(first.get<Duration>()->asNanoseconds() == 100ns);
    CHECK(second.get<Rtime>()->asNanoseconds() == 100ns);
    CHECK(second.get<Duration>()->asNanoseconds() == 0ns);
    REQUIRE(first.has<CartesianPosition>());
    auto first_position = first.get<CartesianPosition>();
    CHECK(first_position.get<X>() == END_VALUE);
    CHECK(first_position.get<Y>() == END_VALUE);
    CHECK(first_position.get<Z>() == END_VALUE);
    REQUIRE(second.has<CartesianPosition>());
    auto second_position = second.get<CartesianPosition>();
    CHECK(second_position.get<X>() == END_VALUE);
    CHECK(second_position.get<Y>() == END_VALUE);
    CHECK(second_position.get<Z>() == END_VALUE);
  }
}

TEST_CASE("Split outside block boundaries throw") {
  auto const START_VALUE = .0f;
  auto const END_VALUE = .1f;
  AudioBlockFormatObjects prior{CartesianPosition{X{START_VALUE}, Y{START_VALUE}, Z{START_VALUE}}, Rtime{100ns},
                                Duration{100ns}};
  // valid split times are 200ns-300ns
  AudioBlockFormatObjects toSplit{CartesianPosition{X{END_VALUE}, Y{END_VALUE}, Z{END_VALUE}}, Rtime(200ns),
                                  Duration(100ns)};
  CHECK_THROWS(eat::process::split(prior, toSplit, Rtime(199ns)));
  CHECK_THROWS(eat::process::split(prior, toSplit, Rtime(301ns)));
}
