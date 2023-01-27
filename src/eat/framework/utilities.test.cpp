#include "utilities.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace eat::framework;

class HasOutput : public FunctionalAtomicProcess {
 public:
  HasOutput(const std::string &name) : FunctionalAtomicProcess(name), out(add_out_port<DataPort<std::string>>("out")) {}
  void process() override {}

 private:
  DataPortPtr<std::string> out;
};

class HasInput : public FunctionalAtomicProcess {
 public:
  HasInput(const std::string &name) : FunctionalAtomicProcess(name), in(add_in_port<DataPort<std::string>>("in")) {}
  void process() override {}

 private:
  DataPortPtr<std::string> in;
};

TEST_CASE("output_connections") {
  Graph g;
  auto out = g.add_process<HasOutput>("out");
  auto in1 = g.add_process<HasInput>("in1");
  auto in2 = g.add_process<HasInput>("in2");

  g.connect(out->get_out_port("out"), in1->get_in_port("in"));
  g.connect(out->get_out_port("out"), in2->get_in_port("in"));

  auto connections = output_connections(g, out);
  REQUIRE(connections.size() == 2);

  bool order = connections.at(0).downstream_process == in1;
  auto con1 = order ? connections.at(0) : connections.at(1);
  auto con2 = order ? connections.at(1) : connections.at(0);
  REQUIRE(con1.upstream_process == out);
  REQUIRE(con1.downstream_process == in1);
  REQUIRE(con1.upstream_port == out->get_out_port("out"));
  REQUIRE(con1.downstream_port == in1->get_in_port("in"));
  REQUIRE(con2.upstream_process == out);
  REQUIRE(con2.downstream_process == in2);
  REQUIRE(con2.upstream_port == out->get_out_port("out"));
  REQUIRE(con2.downstream_port == in2->get_in_port("in"));

  REQUIRE(output_connections(g, in1).size() == 0);
  REQUIRE(output_connections(g, in2).size() == 0);
}

TEST_CASE("input_connections") {
  Graph g;
  auto out = g.add_process<HasOutput>("out");
  auto in1 = g.add_process<HasInput>("in1");
  auto in2 = g.add_process<HasInput>("in2");

  g.connect(out->get_out_port("out"), in1->get_in_port("in"));
  g.connect(out->get_out_port("out"), in2->get_in_port("in"));

  {
    auto connections = input_connections(g, in1);
    REQUIRE(connections.size() == 1);
    REQUIRE(connections.at(0).upstream_process == out);
    REQUIRE(connections.at(0).downstream_process == in1);
    REQUIRE(connections.at(0).upstream_port == out->get_out_port("out"));
    REQUIRE(connections.at(0).downstream_port == in1->get_in_port("in"));
  }

  {
    auto connections = input_connections(g, in2);
    REQUIRE(connections.size() == 1);
    REQUIRE(connections.at(0).upstream_process == out);
    REQUIRE(connections.at(0).downstream_process == in2);
    REQUIRE(connections.at(0).upstream_port == out->get_out_port("out"));
    REQUIRE(connections.at(0).downstream_port == in2->get_in_port("in"));
  }

  REQUIRE(input_connections(g, out).size() == 0);
}
