#include <catch2/catch_test_macros.hpp>

#include "eat/framework/evaluate.hpp"
#include "eat/framework/process.hpp"
#include "eat/framework/utility_processes.hpp"
#include "utilities.test.hpp"

using namespace eat::framework;

/// one input and one output
class FunctionalInOut : public FunctionalAtomicProcess {
 public:
  FunctionalInOut(const std::string &name)
      : FunctionalAtomicProcess(name),
        in(add_in_port<DataPort<std::string>>("in")),
        out(add_out_port<DataPort<std::string>>("out")) {}

  virtual void process() override { out->set_value(format_str(name(), {in->get_value()})); }

 private:
  DataPortPtr<std::string> in;
  DataPortPtr<std::string> out;
};

/// one input and two outputs
class FunctionalSplit : public FunctionalAtomicProcess {
 public:
  FunctionalSplit(const std::string &name)
      : FunctionalAtomicProcess(name),
        in(add_in_port<DataPort<std::string>>("in")),
        out1(add_out_port<DataPort<std::string>>("out1")),
        out2(add_out_port<DataPort<std::string>>("out2")) {}

  virtual void process() override {
    out1->set_value(format_str(name() + ".out1", {in->get_value()}));
    out2->set_value(format_str(name() + ".out2", {in->get_value()}));
  }

 private:
  DataPortPtr<std::string> in;
  DataPortPtr<std::string> out1;
  DataPortPtr<std::string> out2;
};

/// two inputs and one output
class FunctionalCombine : public FunctionalAtomicProcess {
 public:
  FunctionalCombine(const std::string &name)
      : FunctionalAtomicProcess(name),
        in1(add_in_port<DataPort<std::string>>("in1")),
        in2(add_in_port<DataPort<std::string>>("in2")),
        out(add_out_port<DataPort<std::string>>("out")) {}

  virtual void process() override { out->set_value(format_str(name(), {in1->get_value(), in2->get_value()})); }

 private:
  DataPortPtr<std::string> in1;
  DataPortPtr<std::string> in2;
  DataPortPtr<std::string> out;
};

/// composite with one input and one output
class CompositeInOut : public CompositeProcess {
 public:
  CompositeInOut(const std::string &name)
      : CompositeProcess(name),
        in(add_in_port<DataPort<std::string>>("in")),
        out(add_out_port<DataPort<std::string>>("out")),
        a(std::make_shared<FunctionalInOut>("c.a")),
        b(std::make_shared<FunctionalInOut>("c.b")) {
    register_process(a);
    register_process(b);
    connect(a->get_out_port("out"), b->get_in_port("in"));
    connect(in, a->get_in_port("in"));
    connect(b->get_out_port("out"), out);
  }

 private:
  DataPortPtr<std::string> in;
  DataPortPtr<std::string> out;
  std::shared_ptr<FunctionalInOut> a;
  std::shared_ptr<FunctionalInOut> b;
};

TEST_CASE("functional atomic then composite") {
  Graph g;

  auto in = g.add_process<DataSource<std::string>>("in", "in");

  auto a = std::make_shared<FunctionalInOut>("a");
  g.register_process(a);

  auto c = std::make_shared<CompositeInOut>("c");
  g.register_process(c);

  auto out = g.add_process<DataSink<std::string>>("out");

  g.connect(in->get_out_port("out"), a->get_in_port("in"));
  g.connect(a->get_out_port("out"), c->get_in_port("in"));
  g.connect(c->get_out_port("out"), out->get_in_port("in"));

  validate(g);

  evaluate(g);

  REQUIRE(out->get_value() == "c.b(c.a(a(in)))");
}

TEST_CASE("functional composite then atomic") {
  Graph g;

  auto in = g.add_process<DataSource<std::string>>("in", "in");

  auto c = std::make_shared<CompositeInOut>("c");
  g.register_process(c);

  auto a = std::make_shared<FunctionalInOut>("a");
  g.register_process(a);

  auto out = g.add_process<DataSink<std::string>>("out");

  g.connect(in->get_out_port("out"), c->get_in_port("in"));
  g.connect(c->get_out_port("out"), a->get_in_port("in"));
  g.connect(a->get_out_port("out"), out->get_in_port("in"));

  validate(g);

  evaluate(g);

  REQUIRE(out->get_value() == "a(c.b(c.a(in)))");
}

TEST_CASE("functional split combine") {
  Graph g;

  auto in = g.add_process<DataSource<std::string>>("in", "in");

  auto split = std::make_shared<FunctionalSplit>("split");
  g.register_process(split);

  auto a1 = std::make_shared<FunctionalInOut>("a1");
  g.register_process(a1);
  auto a2 = std::make_shared<FunctionalInOut>("a2");
  g.register_process(a2);

  auto combine = std::make_shared<FunctionalCombine>("combine");
  g.register_process(combine);

  auto out = g.add_process<DataSink<std::string>>("out");

  g.connect(in->get_out_port("out"), split->get_in_port("in"));
  g.connect(split->get_out_port("out1"), a1->get_in_port("in"));
  g.connect(split->get_out_port("out2"), a2->get_in_port("in"));
  g.connect(a1->get_out_port("out"), combine->get_in_port("in1"));
  g.connect(a2->get_out_port("out"), combine->get_in_port("in2"));
  g.connect(combine->get_out_port("out"), out->get_in_port("in"));

  validate(g);

  evaluate(g);

  REQUIRE(out->get_value() == "combine(a1(split.out1(in)), a2(split.out2(in)))");
}

TEST_CASE("functional branch") {
  Graph g;

  auto in = g.add_process<DataSource<std::string>>("in", "in");

  auto a = std::make_shared<FunctionalInOut>("a");
  g.register_process(a);

  auto a1 = std::make_shared<FunctionalInOut>("a1");
  g.register_process(a1);
  auto a2 = std::make_shared<FunctionalInOut>("a2");
  g.register_process(a2);

  auto combine = std::make_shared<FunctionalCombine>("combine");
  g.register_process(combine);

  auto out = g.add_process<DataSink<std::string>>("out");

  g.connect(in->get_out_port("out"), a->get_in_port("in"));
  g.connect(a->get_out_port("out"), a1->get_in_port("in"));
  g.connect(a->get_out_port("out"), a2->get_in_port("in"));
  g.connect(a1->get_out_port("out"), combine->get_in_port("in1"));
  g.connect(a2->get_out_port("out"), combine->get_in_port("in2"));
  g.connect(combine->get_out_port("out"), out->get_in_port("in"));

  validate(g);

  evaluate(g);

  REQUIRE(out->get_value() == "combine(a1(a(in)), a2(a(in)))");
}
