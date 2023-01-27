#include "time_utils.hpp"

#include <catch2/catch_test_macros.hpp>

#include "../utilities/ostream_operators.hpp"

using namespace eat::process;

TEST_CASE("try_match_time_type") {
  using std::chrono::nanoseconds;
  // converts to ns if possible
  REQUIRE(try_match_time_type(nanoseconds{1}, adm::RationalTime{1, 2}) == nanoseconds{500000000});
  // not possible
  REQUIRE(try_match_time_type(nanoseconds{1}, adm::RationalTime{1, 3}) == adm::FractionalTime{1, 3});

  // converts to the same denominator if possible
  REQUIRE(try_match_time_type(adm::FractionalTime{1, 2}, adm::RationalTime{1, 1}) == adm::FractionalTime{2, 2});
  REQUIRE(try_match_time_type(adm::FractionalTime{500, 48000}, adm::RationalTime{100, 24000}) ==
          adm::FractionalTime{200, 48000});
  // not possible
  REQUIRE(try_match_time_type(adm::FractionalTime{1, 2}, adm::RationalTime{1, 3}) == adm::FractionalTime{1, 3});
  REQUIRE(try_match_time_type(adm::FractionalTime{500, 48000}, adm::RationalTime{100, 44100}) ==
          adm::FractionalTime{1, 441});
}
