#include "eat/framework/utility_processes.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace eat::framework;
using namespace Catch::Matchers;

TEST_CASE("DataSource, value in constructor") {
  DataSource<int> ds("ds", 42);

  ds.process();
  CHECK(ds.get_out_port<DataPort<int>>("out")->get_value() == 42);

  CHECK_THROWS_WITH(ds.process(), Equals("value has not been set"));
}

TEST_CASE("DataSource, value not in constructor") {
  DataSource<int> ds("ds");

  CHECK_THROWS_WITH(ds.process(), Equals("value has not been set"));

  ds.set_value(42);

  ds.process();

  CHECK(ds.get_out_port<DataPort<int>>("out")->get_value() == 42);

  CHECK_THROWS_WITH(ds.process(), Equals("value has not been set"));
}

TEST_CASE("DataSink") {
  DataSink<int> ds("ds");

  ds.get_in_port<DataPort<int>>("in")->set_value(42);

  CHECK_THROWS_WITH(ds.get_value(), Equals("value has not been set"));

  ds.process();

  CHECK(ds.get_value() == 42);
}

TEST_CASE("NullSink") {
  NullSink<int> ds("ds");

  ds.get_in_port<DataPort<int>>("in")->set_value(42);

  ds.process();
}
