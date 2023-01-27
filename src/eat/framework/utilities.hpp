#pragma once
#include <set>

#include "eat/framework/exceptions.hpp"
#include "eat/framework/process.hpp"

namespace eat::framework {

// same as std::dynamic_pointer_cast, but checks the result is valid
template <class T, class U>
std::shared_ptr<T> checked_dynamic_pointer_cast(U &&ptr) {
  auto out = std::dynamic_pointer_cast<T>(std::forward<U>(ptr));
  if (!out) throw AssertionError("tried to cast shared_ptr to wrong type");
  return out;
}

// functions that make working with graphs a bit nicer. generally these are
// hideously inefficient for what they do, but could be improved with a better
// Graph API, or some acceleration structure on top of Graph
//
// these also assume that the graph has been flattened

// ports connected  to port
inline std::vector<PortPtr> connected_ports(const Graph &g, const PortPtr &port) {
  std::vector<PortPtr> out;
  for (auto &pair : g.get_port_inputs())
    if (pair.second == port) out.push_back(pair.first);
  return out;
}

/// get the process associated with an output port. may return null
inline ProcessPtr process_for_out_port(const Graph &g, const PortPtr &port) {
  for (auto &other_process : g.get_processes())
    for (auto &[out_port_name, out_port] : other_process->get_out_port_map())
      if (out_port == port) return other_process;

  return {};
}

/// get the process associated with an input port. may return null
inline ProcessPtr process_for_in_port(const Graph &g, const PortPtr &port) {
  for (auto &other_process : g.get_processes())
    for (auto &[out_port_name, out_port] : other_process->get_in_port_map())
      if (out_port == port) return other_process;

  return {};
}

inline bool is_streaming(const ProcessPtr &p) {
  return static_cast<bool>(std::dynamic_pointer_cast<StreamingAtomicProcess>(p));
}

inline bool is_streaming(const PortPtr &p) { return static_cast<bool>(std::dynamic_pointer_cast<StreamPortBase>(p)); }

struct Connection {
  ProcessPtr upstream_process;
  ProcessPtr downstream_process;
  PortPtr upstream_port;
  PortPtr downstream_port;

  bool is_streaming() const { return eat::framework::is_streaming(upstream_port); }
};

inline std::vector<Connection> output_connections(const Graph &g, const ProcessPtr &process) {
  std::vector<Connection> out;

  for (auto &[name, port] : process->get_out_port_map())
    for (auto &other_process : g.get_processes())
      for (auto &[other_name, other_port] : other_process->get_in_port_map()) {
        auto it = g.get_port_inputs().find(other_port);
        if (it != g.get_port_inputs().end() && it->second == port)
          out.push_back({process, other_process, port, other_port});
      }

  return out;
}

inline std::vector<Connection> input_connections(const Graph &g, const ProcessPtr &process) {
  std::vector<Connection> out;

  for (auto &[name, port] : process->get_in_port_map()) {
    std::vector<PortPtr> ports = {port};

    auto it = g.get_port_inputs().find(port);
    always_assert(it != g.get_port_inputs().end(), "port not connected");
    PortPtr upstream_port = it->second;

    ProcessPtr upstream_process = process_for_out_port(g, upstream_port);
    always_assert(static_cast<bool>(upstream_process), "port not associated with process");

    out.push_back({upstream_process, process, upstream_port, port});
  }

  return out;
}

}  // namespace eat::framework
