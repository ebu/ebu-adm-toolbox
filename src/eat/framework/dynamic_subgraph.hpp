#pragma once

#include "eat/framework/evaluate.hpp"
#include "eat/framework/process.hpp"
#include "exec_steps.hpp"

namespace eat::framework {

class ParentDataPortBase : public FunctionalAtomicProcess {
 public:
  ParentDataPortBase(const std::string &name, bool input_) : FunctionalAtomicProcess(name), input(input_) {}

  void process() override {}

  bool input;
  DataPortBasePtr port;
};

template <typename T>
class ParentDataInput : public ParentDataPortBase {
 public:
  ParentDataInput(const std::string &name) : ParentDataPortBase(name, true) { port = add_out_port<DataPort<T>>("out"); }
};

template <typename T>
class ParentDataOutput : public ParentDataPortBase {
 public:
  ParentDataOutput(const std::string &name) : ParentDataPortBase(name, false) { port = add_in_port<DataPort<T>>("in"); }
};

class ParentStreamPortBase : public StreamingAtomicProcess {
 public:
  ParentStreamPortBase(const std::string &name, bool input_) : StreamingAtomicProcess(name), input(input_) {}

  void initialise() override {}
  void process() override {}
  void finalise() override {}

  bool input;
  StreamPortBasePtr port;
};

template <typename T>
class ParentStreamInput : public ParentStreamPortBase {
 public:
  ParentStreamInput(const std::string &name) : ParentStreamPortBase(name, true) {
    port = add_out_port<StreamPort<T>>("out");
  }
};

template <typename T>
class ParentStreamOutput : public ParentStreamPortBase {
 public:
  ParentStreamOutput(const std::string &name) : ParentStreamPortBase(name, false) {
    port = add_in_port<StreamPort<T>>("in");
  }
};

/// a streaming process which executes a subgraph which is dynamically created
/// based on it's inputs
///
/// To use this, subclass it and implement only build_subgraph. This will be
/// called during the initialise() phase, and should construct and return a
/// graph to run based on the data inputs.
///
/// This subgraph can be connected to the input and output ports of the parent
/// by adding special processes with corresponding names.
///
/// For example, ParentDataInput<T>{"port_name"} connects to an input data port
/// with type T named port_name in the parent. This has a `port` member which
/// can be connected to other ports in the subgraph (these are named out and in
/// for inputs and outputs so can also be accessed with
/// get_in_port/get_out_port). The following special processes are defined:
///
/// - ParentDataInput -- data input of the parent
/// - ParentDataOutput -- data output of the parent
/// - ParentStreamInput -- stream input of the parent
/// - ParentStreamOutput -- stream output of the parent
///
/// this currently only supports subgraphs that have at least one streaming
/// process, and where all streaming processes can be ran as a single
/// subgraph (i.e. there should be no data dependencies between streaming
/// processes).
class DynamicSubgraph : public StreamingAtomicProcess {
 public:
  using StreamingAtomicProcess::StreamingAtomicProcess;

  virtual void initialise() override {
    subgraph = build_subgraph();

    steps = plan(*subgraph).steps();

    // partition the plan into three parts: n non-streaming steps, a streaming
    // step (stored in streaming_step / streaming_step_idx), and n
    // non-streaming steps. if the plan doesn't match this pattern, it's not
    // possible to run it inside a regular process, so should be split up
    {
      size_t i = 0;
      for (; i < steps.size(); i++) {
        streaming_step = std::dynamic_pointer_cast<ExecStreamingSubgraph>(steps[i]);
        if (streaming_step) break;
      }

      if (i == steps.size()) throw std::runtime_error{"found no streaming subgraph"};

      streaming_step_idx = i;
      i++;

      for (; i < steps.size(); i++) {
        if (std::dynamic_pointer_cast<ExecStreamingSubgraph>(steps[i]))
          throw std::runtime_error{"found more than one streaming subgraph"};
      }
    }

    // find parent port processes in the subgraph, and save pairs of ports to copy between
    for (auto &process : subgraph->get_processes()) {
      if (auto data_process = std::dynamic_pointer_cast<ParentDataPortBase>(process); data_process) {
        if (data_process->input)
          add_port(data_inputs, data_process);
        else
          add_port(data_outputs, data_process);
      } else if (auto stream_process = std::dynamic_pointer_cast<ParentStreamPortBase>(process); stream_process) {
        if (stream_process->input)
          add_port(stream_inputs, stream_process);
        else
          add_port(stream_outputs, stream_process);
      }
    }

    // copy data inputs into subgraph, run the non-streaming steps, then the
    // initialisation part of the streaming step
    for (auto &[outer_data_input, inner_data_input] : data_inputs) outer_data_input->move_to(*inner_data_input);
    for (size_t i = 0; i < streaming_step_idx; i++) steps[i]->run();
    streaming_step->run_initialise();
  }

  virtual void process() override {
    // run one step of the streaming processes, copying the inputs and outputs before and after

    for (auto &[outer_stream_input, inner_stream_input] : stream_inputs) {
      outer_stream_input->move_to(*inner_stream_input);
      outer_stream_input->clear();
    }

    streaming_step->run_run();

    for (auto &[outer_stream_output, inner_stream_output] : stream_outputs) {
      inner_stream_output->move_to(*outer_stream_output);
      inner_stream_output->clear();
    }
  }

  virtual void finalise() override {
    // at this point the input and output streaming ports must have been closed
    // (as the parent graph has stopped running process), but some of our
    // streaming processes may not have finished, so keep running until they
    // have
    while (streaming_step->runnable()) process();

    // finalise the streaming steps, run the later non-streaming steps, and
    // copy the data outputs
    streaming_step->run_finalise();
    for (size_t i = streaming_step_idx + 1; i < steps.size(); i++) steps[i]->run();
    for (auto &[outer_data_output, inner_data_output] : data_outputs) inner_data_output->move_to(*outer_data_output);
  }

 protected:
  virtual GraphPtr build_subgraph() = 0;

 private:
  template <typename PortT>
  using PortPairVector = std::vector<std::pair<std::shared_ptr<PortT>, std::shared_ptr<PortT>>>;

  /// given a process in our subgraph (some subclass of ParentDataPortBase or
  /// ParentStreamPortBase), find a port in this process which matches it (same
  /// type and input/output), and add it to ports_vec
  template <typename PortT, typename ProcessT>
  void add_port(PortPairVector<PortT> &ports_vec, ProcessT process) {
    auto our_ports = process->input ? get_in_port_map() : get_out_port_map();

    auto it = our_ports.find(process->name());

    if (it == our_ports.end())
      throw std::runtime_error("could not find " + (process->input ? std::string{"input"} : std::string{"output"}) +
                               " port named " + process->name());

    // from generic port to data/stream
    auto our_port_t = std::dynamic_pointer_cast<PortT>(it->second);
    if (!our_port_t)
      throw std::runtime_error("data/stream port mismatch between inner/outer ports named " + process->name());

    if (!our_port_t->compatible(process->port))
      throw std::runtime_error("inner/outer port named " + process->name() + " do not have the same type");

    ports_vec.emplace_back(our_port_t, process->port);
  }

  GraphPtr subgraph;
  std::vector<ExecStepPtr> steps;
  std::shared_ptr<ExecStreamingSubgraph> streaming_step;
  size_t streaming_step_idx;

  // pairs of outside ports and inside ports to copy between
  PortPairVector<DataPortBase> data_inputs;
  PortPairVector<DataPortBase> data_outputs;
  PortPairVector<StreamPortBase> stream_inputs;
  PortPairVector<StreamPortBase> stream_outputs;
};
}  // namespace eat::framework
