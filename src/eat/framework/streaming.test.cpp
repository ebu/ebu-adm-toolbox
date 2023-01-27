#include <catch2/catch_test_macros.hpp>

#include "eat/framework/evaluate.hpp"
#include "eat/framework/process.hpp"
#include "eat/framework/utility_processes.hpp"
#include "utilities.test.hpp"

using namespace eat::framework;

/// has a data output and a stream output that sends n_messages messages
class StreamAndDataOut : public StreamingAtomicProcess {
 public:
  StreamAndDataOut(const std::string &name, int n_messages = 1)
      : StreamingAtomicProcess(name),
        out_data(add_out_port<DataPort<std::string>>("out_data")),
        out_stream(add_out_port<StreamPort<std::string>>("out_stream")),
        n_messages(n_messages) {}

  void process() override {
    if (message_idx < n_messages) {
      out_stream->push(name() + ".stream" + std::to_string(message_idx));
      message_idx++;
      if (message_idx == n_messages) out_stream->close();
    }
  }

  void finalise() override { out_data->set_value(name() + ".data"); }

 private:
  DataPortPtr<std::string> out_data;
  StreamPortPtr<std::string> out_stream;
  int n_messages;
  int message_idx = 0;
};

/// has a data and stream input; these are forwarded on two data ports for testing
class StreamAndDataIn : public StreamingAtomicProcess {
 public:
  StreamAndDataIn(const std::string &name)
      : StreamingAtomicProcess(name),
        in_data(add_in_port<DataPort<std::string>>("in_data")),
        in_stream(add_in_port<StreamPort<std::string>>("in_stream")),
        out_data(add_out_port<DataPort<std::string>>("out_data")),
        out_stream(add_out_port<DataPort<std::string>>("out_stream")) {}

  void initialise() override { out_data->set_value(format_str(name() + ".data", {in_data->get_value()})); }

  void process() override {
    if (in_stream->available()) stream_inputs.push_back(in_stream->pop());
  }

  void finalise() override { out_stream->set_value(format_str(name() + ".stream", stream_inputs)); }

 private:
  DataPortPtr<std::string> in_data;
  StreamPortPtr<std::string> in_stream;
  DataPortPtr<std::string> out_data;
  DataPortPtr<std::string> out_stream;

  std::vector<std::string> stream_inputs;
};

TEST_CASE("streaming split simple") {
  Graph g;

  // two processes connected together with both stream and data ports

  auto out = std::make_shared<StreamAndDataOut>("out", 2);
  g.register_process(out);

  auto in = std::make_shared<StreamAndDataIn>("in");
  g.register_process(in);

  auto out_data = g.add_process<DataSink<std::string>>("out_data");
  auto out_stream = g.add_process<DataSink<std::string>>("out_stream");

  g.connect(out->get_out_port("out_stream"), in->get_in_port("in_stream"));
  g.connect(out->get_out_port("out_data"), in->get_in_port("in_data"));

  g.connect(in->get_out_port("out_stream"), out_stream->get_in_port("in"));
  g.connect(in->get_out_port("out_data"), out_data->get_in_port("in"));

  evaluate(g);

  REQUIRE(out_data->get_value() == "in.data(out.data)");
  REQUIRE(out_stream->get_value() == "in.stream(out.stream0, out.stream1)");
}

TEST_CASE("streaming split cross") {
  Graph g;

  // a graph which forces either out1->in1 or out2->in2 to be split up.
  //
  // with -s-> denoting a streaming connections and -d-> a data connection:
  //
  // out1 -s-> in1
  // out1 -d-> in2
  // out2 -s-> in2
  // out2 -d-> in1

  auto out1 = std::make_shared<StreamAndDataOut>("out1", 2);
  g.register_process(out1);
  auto out2 = std::make_shared<StreamAndDataOut>("out2", 2);
  g.register_process(out2);

  auto in1 = std::make_shared<StreamAndDataIn>("in1");
  g.register_process(in1);
  auto in2 = std::make_shared<StreamAndDataIn>("in2");
  g.register_process(in2);

  auto in1_out_stream = g.add_process<DataSink<std::string>>("in1_out_stream");
  auto in2_out_stream = g.add_process<DataSink<std::string>>("in2_out_stream");
  auto in1_out_data = g.add_process<DataSink<std::string>>("in1_out_data");
  auto in2_out_data = g.add_process<DataSink<std::string>>("in2_out_data");

  g.connect(out1->get_out_port("out_stream"), in1->get_in_port("in_stream"));
  g.connect(out2->get_out_port("out_stream"), in2->get_in_port("in_stream"));

  g.connect(out1->get_out_port("out_data"), in2->get_in_port("in_data"));
  g.connect(out2->get_out_port("out_data"), in1->get_in_port("in_data"));

  g.connect(in1->get_out_port("out_stream"), in1_out_stream->get_in_port("in"));
  g.connect(in2->get_out_port("out_stream"), in2_out_stream->get_in_port("in"));
  g.connect(in1->get_out_port("out_data"), in1_out_data->get_in_port("in"));
  g.connect(in2->get_out_port("out_data"), in2_out_data->get_in_port("in"));

  evaluate(g);

  REQUIRE(in1_out_data->get_value() == "in1.data(out2.data)");
  REQUIRE(in2_out_data->get_value() == "in2.data(out1.data)");

  REQUIRE(in1_out_stream->get_value() == "in1.stream(out1.stream0, out1.stream1)");
  REQUIRE(in2_out_stream->get_value() == "in2.stream(out2.stream0, out2.stream1)");
}

TEST_CASE("streaming fork") {
  Graph g;

  // one streaming output connected to two inputs, that can run simultaneously

  auto in1_in = g.add_process<DataSource<std::string>>("in1_in", "in1");
  auto in2_in = g.add_process<DataSource<std::string>>("in2_in", "in2");

  auto out = std::make_shared<StreamAndDataOut>("out", 2);
  g.register_process(out);

  auto in1 = std::make_shared<StreamAndDataIn>("in1");
  g.register_process(in1);
  auto in2 = std::make_shared<StreamAndDataIn>("in2");
  g.register_process(in2);

  auto in1_out_stream = g.add_process<DataSink<std::string>>("in1_out_stream");
  auto in2_out_stream = g.add_process<DataSink<std::string>>("in2_out_stream");
  auto in1_out_data = g.add_process<DataSink<std::string>>("in1_out_data");
  auto in2_out_data = g.add_process<DataSink<std::string>>("in2_out_data");
  auto out_out_data = g.add_process<NullSink<std::string>>("out_out_data");

  g.connect(in1_in->get_out_port("out"), in1->get_in_port("in_data"));
  g.connect(in2_in->get_out_port("out"), in2->get_in_port("in_data"));

  g.connect(out->get_out_port("out_stream"), in1->get_in_port("in_stream"));
  g.connect(out->get_out_port("out_stream"), in2->get_in_port("in_stream"));

  g.connect(in1->get_out_port("out_stream"), in1_out_stream->get_in_port("in"));
  g.connect(in2->get_out_port("out_stream"), in2_out_stream->get_in_port("in"));
  g.connect(in1->get_out_port("out_data"), in1_out_data->get_in_port("in"));
  g.connect(in2->get_out_port("out_data"), in2_out_data->get_in_port("in"));
  g.connect(out->get_out_port("out_data"), out_out_data->get_in_port("in"));

  evaluate(g);

  REQUIRE(in1_out_stream->get_value() == "in1.stream(out.stream0, out.stream1)");
  REQUIRE(in2_out_stream->get_value() == "in2.stream(out.stream0, out.stream1)");

  REQUIRE(in1_out_data->get_value() == "in1.data(in1)");
  REQUIRE(in2_out_data->get_value() == "in2.data(in2)");
}

/// has a data and 2 stream inputs; these are forwarded on two data ports for testing
class TwoStreamAndDataIn : public StreamingAtomicProcess {
 public:
  TwoStreamAndDataIn(const std::string &name)
      : StreamingAtomicProcess(name),
        in_data(add_in_port<DataPort<std::string>>("in_data")),
        in_stream1(add_in_port<StreamPort<std::string>>("in_stream1")),
        in_stream2(add_in_port<StreamPort<std::string>>("in_stream2")),
        out_data(add_out_port<DataPort<std::string>>("out_data")),
        out_stream(add_out_port<DataPort<std::string>>("out_stream")) {}

  void initialise() override { out_data->set_value(format_str(name() + ".data", {in_data->get_value()})); }

  void process() override {
    if (in_stream1->available()) stream1_inputs.push_back(in_stream1->pop());
    if (in_stream2->available()) stream2_inputs.push_back(in_stream2->pop());
  }

  void finalise() override {
    out_stream->set_value(format_str(name(), {
                                                 format_str("stream1", stream1_inputs),
                                                 format_str("stream2", stream2_inputs),
                                             }));
  }

 private:
  DataPortPtr<std::string> in_data;
  StreamPortPtr<std::string> in_stream1;
  StreamPortPtr<std::string> in_stream2;
  DataPortPtr<std::string> out_data;
  DataPortPtr<std::string> out_stream;

  std::vector<std::string> stream1_inputs;
  std::vector<std::string> stream2_inputs;
};

TEST_CASE("streaming split dup") {
  Graph g;

  // two processes connected together with both stream and data ports, with the
  // stream is split in two

  auto out = std::make_shared<StreamAndDataOut>("out", 2);
  g.register_process(out);

  auto in = std::make_shared<TwoStreamAndDataIn>("in");
  g.register_process(in);

  auto in_data = g.add_process<DataSink<std::string>>("in_data");
  auto in_stream = g.add_process<DataSink<std::string>>("in_stream");

  g.connect(out->get_out_port("out_stream"), in->get_in_port("in_stream1"));
  g.connect(out->get_out_port("out_stream"), in->get_in_port("in_stream2"));
  g.connect(out->get_out_port("out_data"), in->get_in_port("in_data"));

  g.connect(in->get_out_port("out_data"), in_data->get_in_port("in"));
  g.connect(in->get_out_port("out_stream"), in_stream->get_in_port("in"));

  evaluate(g);

  REQUIRE(in_data->get_value() == "in.data(out.data)");
  REQUIRE(in_stream->get_value() == "in(stream1(out.stream0, out.stream1), stream2(out.stream0, out.stream1))");
}
