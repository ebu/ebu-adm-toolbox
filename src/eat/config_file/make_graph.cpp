#include "make_graph.hpp"

#include <string>

#include "eat/framework/exceptions.hpp"
#include "utilities.hpp"

namespace eat::config_file {
namespace {

void make_pipeline(framework::Graph &graph, std::vector<nlohmann::json> config) {
  using ports = std::vector<std::string>;

  framework::ProcessPtr last_process;
  ports last_output_ports;

  for (auto &process_config : config) {
    if (!process_config.is_object()) throw std::runtime_error("expected object");

    auto in_ports = get<ports>(process_config, "in_ports", {});
    auto out_ports = get<ports>(process_config, "out_ports", {});

    framework::ProcessPtr process = make_process(process_config);

    check_empty(process_config);

    graph.register_process(process);
    if (!last_process && in_ports.size()) throw std::runtime_error("cannot specify in_ports in first process");

    if (last_output_ports.size() != in_ports.size())
      throw std::runtime_error(
          "input ports of one process must be the same length as the output ports of the previous process");

    for (size_t i = 0; i < in_ports.size(); i++)
      graph.connect(last_process->get_out_port(last_output_ports[i]), process->get_in_port(in_ports[i]));

    last_process = process;
    last_output_ports = out_ports;
  }

  if (last_output_ports.size()) throw std::runtime_error("last process should not have out_ports");
}

/// find a process with a given name
framework::ProcessPtr find_process(const framework::Graph &g, const std::string &name) {
  for (auto &process : g.get_processes())
    if (process->name() == name) return process;

  throw std::runtime_error("could not find process named " + name);
}

using port_ref = std::pair<std::string, std::string>;

/// parse a port reference of the form process.port
port_ref parse_port_ref(const std::string &name) {
  size_t p = name.rfind('.');
  if (p == std::string::npos || p == 0 || p == name.size() - 1)
    throw std::runtime_error("port must be of form process.port");

  std::string process_name = name.substr(0, p);
  std::string port_name = name.substr(p + 1, name.size());
  return {process_name, port_name};
}

framework::PortPtr get_out_port(const framework::Graph &g, const port_ref &port) {
  auto &[process_name, port_name] = port;
  return find_process(g, process_name)->get_out_port(port_name);
}

framework::PortPtr get_in_port(const framework::Graph &g, const port_ref &port) {
  auto &[process_name, port_name] = port;
  return find_process(g, process_name)->get_in_port(port_name);
}

framework::Graph make_graph_v0(nlohmann::json &config) {
  framework::Graph g;
  auto processes = get<std::vector<nlohmann::json>>(config, "processes");
  make_pipeline(g, processes);

  auto connections = get<std::vector<std::pair<std::string, std::string>>>(config, "connections", {});

  for (auto &[out_port_name, in_port_name] : connections)
    g.connect(get_out_port(g, parse_port_ref(out_port_name)), get_in_port(g, parse_port_ref(in_port_name)));

  return g;
}

}  // namespace

framework::Graph make_graph(nlohmann::json config) {
  const int max_version = 0;

  if (!config.contains("version"))
    throw std::runtime_error{"config has no version attribute; current version: " + std::to_string(max_version)};
  auto version = get<int>(config, "version");

  framework::Graph g;
  if (version == 0)
    g = make_graph_v0(config);
  else
    throw std::runtime_error{"don't know how to read version " + std::to_string(version) +
                             " config files; current version: " + std::to_string(max_version)};

  framework::always_assert(version <= max_version, "update max_version");

  check_empty(config);

  return g;
}

}  // namespace eat::config_file
