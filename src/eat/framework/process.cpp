#include "eat/framework/process.hpp"

#include <stdexcept>

namespace eat::framework {

static void check_connection(const PortPtr &a, const PortPtr &b) {
  if (!a || !b) throw std::runtime_error("tried to connect a null port");
  if (a == b) throw std::runtime_error("tried to connect a port to itself");

  if (!a->compatible(b)) throw std::runtime_error("tried to connect incompatible ports");
}
// Graph

ProcessPtr Graph::register_process(ProcessPtr process) {
  processes.push_back(process);
  return process;
}

void Graph::connect(const PortPtr &a, const PortPtr &b) {
  check_connection(a, b);

  bool found_a = false;
  bool found_b = false;
  for (auto &process : get_processes()) {
    for (auto &pair : process->get_in_port_map()) {
      if (pair.second == b) found_b = true;
      if (pair.second == a) throw std::runtime_error("cannot connect from an input port of a sub-process");
    }

    for (auto &pair : process->get_out_port_map()) {
      if (pair.second == a) found_a = true;
      if (pair.second == b) throw std::runtime_error("cannot connect to an output port of a sub-process");
    }
  }

  if (!found_a) throw std::runtime_error("cannot connect from an unregistered port");
  if (!found_b) throw std::runtime_error("cannot connect to an unregistered port");

  auto result = port_inputs.emplace(b, a);
  if (!result.second) throw std::runtime_error("multiple inputs specified for port");
}

void CompositeProcess::connect(const PortPtr &a, const PortPtr &b) {
  check_connection(a, b);

  bool found_a = false;
  bool found_b = false;
  for (auto &pair : get_in_port_map()) {
    if (pair.second == a) found_a = true;
    if (pair.second == b) throw std::runtime_error("cannot connect to an input port of the current process");
  }

  for (auto &pair : get_out_port_map()) {
    if (pair.second == b) found_b = true;
    if (pair.second == a) throw std::runtime_error("cannot connect from an output port of the current process");
  }

  for (auto &process : get_processes()) {
    for (auto &pair : process->get_in_port_map()) {
      if (pair.second == b) found_b = true;
      if (pair.second == a) throw std::runtime_error("cannot connect from an input port of a sub-process");
    }

    for (auto &pair : process->get_out_port_map()) {
      if (pair.second == a) found_a = true;
      if (pair.second == b) throw std::runtime_error("cannot connect to an output port of a sub-process");
    }
  }

  if (!found_a) throw std::runtime_error("cannot connect from an unregistered port");
  if (!found_b) throw std::runtime_error("cannot connect to an unregistered port");

  auto result = port_inputs.emplace(b, a);
  if (!result.second) throw std::runtime_error("multiple inputs specified for port");
}

// Process

Process::Process(const std::string &name) : name_(name) {}

std::map<std::string, PortPtr> Process::get_port_map() const {
  std::map<std::string, PortPtr> all_ports;
  for (auto &pair : in_ports) all_ports["in_" + pair.first] = pair.second;
  for (auto &pair : out_ports) all_ports["out_" + pair.first] = pair.second;

  return all_ports;
}

// Port

Port::Port(const std::string &name) : name_(name) {}

}  // namespace eat::framework
