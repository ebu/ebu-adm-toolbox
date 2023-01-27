#pragma once

#include <algorithm>

#include "eat/framework/evaluate.hpp"
#include "utilities.hpp"

namespace eat::framework {

class ExecFunctional : public ExecStep {
 public:
  ExecFunctional(FunctionalAtomicProcessPtr process_) noexcept : process(std::move(process_)) {}

  virtual void run() override { process->process(); }

  virtual std::string description() override { return process->name(); }

 private:
  FunctionalAtomicProcessPtr process;
};

class ExecStreaming : public ExecStep {
 public:
  ExecStreaming(StreamingAtomicProcessPtr process_) noexcept : process(std::move(process_)) {}

  virtual void run() override { process->process(); }

  virtual std::string description() override { return process->name(); }

 private:
  StreamingAtomicProcessPtr process;
};

class ExecCopyData : public ExecStep {
 public:
  ExecCopyData(DataPortBasePtr output_port_, std::vector<DataPortBasePtr> input_ports_)
      : output_port(std::move(output_port_)), input_ports(std::move(input_ports_)) {
    always_assert(input_ports.size(), "must have at least 1 input port");
  }

  virtual void run() override {
    for (size_t i = 0; i < input_ports.size() - 1; i++) {
      output_port->copy_to(*input_ports[i]);
    }
    output_port->move_to(*input_ports[input_ports.size() - 1]);
  }

  virtual std::string description() override { return "copy data from " + output_port->name(); }

 private:
  DataPortBasePtr output_port;
  std::vector<DataPortBasePtr> input_ports;
};

class ExecCopyStream : public ExecStep {
 public:
  ExecCopyStream(StreamPortBasePtr output_port_, std::vector<StreamPortBasePtr> input_ports_)
      : output_port(std::move(output_port_)), input_ports(std::move(input_ports_)) {
    always_assert(input_ports.size(), "must have at least 1 input port");
  }

  virtual void run() override {
    for (size_t i = 0; i < input_ports.size() - 1; i++) {
      output_port->copy_to(*input_ports[i]);
    }
    output_port->move_to(*input_ports[input_ports.size() - 1]);
    output_port->clear();
  }

  virtual std::string description() override { return "copy data from " + output_port->name(); }

 private:
  StreamPortBasePtr output_port;
  std::vector<StreamPortBasePtr> input_ports;
};

// adds ExecCopyStream step to plan to copy/move data to all connected ports
inline void add_stream_copy_to_plan(const Graph &g, std::vector<ExecStepPtr> &plan, const StreamPortBasePtr &port) {
  std::vector<StreamPortBasePtr> ports;
  for (auto &connected_port : connected_ports(g, port)) {
    auto connected_port_stream = checked_dynamic_pointer_cast<StreamPortBase>(connected_port);
    ports.push_back(connected_port_stream);
  }

  if (ports.size()) plan.push_back(std::make_shared<ExecCopyStream>(port, ports));
}

class ExecStreamingSubgraph : public ExecStep {
 public:
  ExecStreamingSubgraph(const Graph &g, const std::set<ProcessPtr> &subgraph) {
    std::set<ProcessPtr> to_run = subgraph;
    std::set<ProcessPtr> ran;

    auto pick_runnable = [&]() {
      for (auto &process : to_run) {
        bool inputs_ran = true;
        for (auto &connection : input_connections(g, process)) {
          if (connection.is_streaming() && ran.find(connection.upstream_process) == ran.end()) {
            inputs_ran = false;
            break;
          }
        }

        if (inputs_ran) return process;
      }
      throw AssertionError("could not find runnable process");
    };

    while (to_run.size()) {
      auto process = pick_runnable();
      ran.insert(process);
      to_run.erase(process);

      auto streaming_process = checked_dynamic_pointer_cast<StreamingAtomicProcess>(process);

      processes.push_back(streaming_process);

      plan.push_back(std::make_shared<ExecStreaming>(streaming_process));

      // add copies to plan for output ports
      for (auto &[name, port] : process->get_out_port_map()) {
        auto streaming_port = std::dynamic_pointer_cast<StreamPortBase>(port);
        if (streaming_port) {
          add_stream_copy_to_plan(g, plan, streaming_port);
        }
      }
    }

    // populate ports
    for (auto &process : subgraph)
      for (auto &[name, port] : process->get_port_map()) {
        auto streaming_port = std::dynamic_pointer_cast<StreamPortBase>(port);
        if (streaming_port) ports.push_back(streaming_port);
      }
  }

  void run_initialise() {
    for (auto &process : processes) process->initialise();
  }

  void run_run() {
    for (auto &step : plan) step->run();
  }

  void run_finalise() {
    for (auto &process : processes) process->finalise();
  }

  bool runnable() {
    return std::any_of(ports.begin(), ports.end(), [](auto &port) { return !port->eof(); });
  }

  virtual void run() override {
    run_initialise();

    do {
      run_run();
    } while (runnable());

    run_finalise();
  }

  virtual std::string description() override {
    std::string ret = "streaming between ";
    for (size_t i = 0; i < processes.size(); i++) {
      if (i > 0) ret += ", ";
      ret += processes.at(i)->name();
    }
    return ret;
  }

  /// get the mean progress for all processes in this subgraph
  std::optional<float> get_progress() {
    float total = 0.0f;
    int n = 0;

    for (auto &process : processes) {
      auto process_progress = process->get_progress();
      if (process_progress) {
        total += *process_progress;
        n++;
      }
    }

    if (n)
      return total / n;
    else
      return std::nullopt;
  }

 private:
  // all streaming ports in this subgraph for checking if we're done
  std::vector<StreamPortBasePtr> ports;
  // for calling initialise/finalise
  std::vector<StreamingAtomicProcessPtr> processes;
  std::vector<ExecStepPtr> plan;
};

}  // namespace eat::framework
