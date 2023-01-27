#include "eat/utilities/to_dot.hpp"

#include <cstdint>
#include <ostream>
#include <string>

#include "eat/framework/utilities.hpp"

using namespace eat::framework;

namespace eat::utilities {

class DotFormatter {
 public:
  DotFormatter(std::ostream &s_, bool recursive_ = true) : s(s_), recursive(recursive_) {}

  void format(const Graph &g) {
    start_block("digraph g");
    line("rankdir=LR;");
    line("node [shape=record,height=.1]");

    add_nodes(g);

    end_block();
  }

 private:
  void add_nodes(const Graph &graph) {
    for (auto &process : graph.get_processes())
      if (recursive)
        add_nodes(graph, process);
      else
        add_node(graph, process);
    add_port_nodes(graph);
    add_connections(graph);
  }

  void add_nodes(const Graph &context, const ProcessPtr &p) {
    auto composite = std::dynamic_pointer_cast<CompositeProcess>(p);
    if (composite) add_nodes(context, composite);

    auto atomic = std::dynamic_pointer_cast<AtomicProcess>(p);
    if (atomic) add_node(context, atomic);
  }

  void add_nodes(const Graph &context, const CompositeProcessPtr &p) {
    (void)context;
    start_block("subgraph cluster_" + element_name("cp", p.get()));
    line("label=\"" + p->name() + "\"");

    for (auto &process : p->get_processes()) add_nodes(*p, process);

    add_port_nodes(*p);

    add_connections(*p);

    end_block();
  }

  // add nodes for ports in connections which don't belong to any sub-process
  void add_port_nodes(const Graph &graph) {
    std::set<PortPtr> unknown_ports;

    for (auto &pair : graph.get_port_inputs()) {
      auto upstream_process = process_for_out_port(graph, pair.second);
      auto downstream_process = process_for_in_port(graph, pair.first);
      if (!upstream_process) unknown_ports.insert(pair.second);
      if (!downstream_process) unknown_ports.insert(pair.first);
    }

    for (auto &port : unknown_ports) {
      std::string n = element_name("po", port.get());
      port_str[port] = n;
      line(n + "[label=\"" + port->name() + "\",style=rounded];");
    }
  }

  void add_node(const Graph &context, const ProcessPtr &p) {
    std::vector<PortPtr> in_ports;
    std::vector<PortPtr> out_ports;
    std::vector<PortPtr> unknown_ports;

    // classify the ports into in/out/unknown
    auto &port_inputs = context.get_port_inputs();
    for (auto &pair : p->get_port_map()) {
      auto &port = pair.second;
      if (port_inputs.find(port) != port_inputs.end())
        in_ports.push_back(port);
      else {
        bool found_downstream = false;
        for (auto &conn : port_inputs)
          if (conn.second == port) {
            found_downstream = true;
            break;
          }
        if (found_downstream)
          out_ports.push_back(port);
        else {
          if (port->name().rfind("in_", 0) == 0)
            in_ports.push_back(port);
          else if (port->name().rfind("out_", 0) == 0)
            out_ports.push_back(port);
          else
            unknown_ports.push_back(port);
        }
      }
    }

    auto format_port_str = [](const std::vector<PortPtr> &ports, const std::vector<PortPtr> &extra = {}) {
      std::string out;

      out += "{";
      for (auto &port : ports) {
        if (out.size() > 1) out += "|";
        out += "<" + element_name("po", port.get()) + ">" + port->name();
      }

      for (auto &port : extra) {
        if (out.size() > 1) out += "|";
        out += "<" + element_name("po", port.get()) + ">[" + port->name() + "]";
      }
      out += "}";
      return out;
    };

    std::string in_port_str = format_port_str(in_ports);
    std::string out_port_str = format_port_str(out_ports, unknown_ports);

    line(element_name("pr", p.get()) + "[label = \"{" + in_port_str + " | " + p->name() + " | " + out_port_str +
         "}\"];");

    // register port node names
    for (auto &pair : p->get_port_map()) {
      auto &port = pair.second;
      port_str[port] = element_name("pr", p.get()) + ":" + element_name("po", port.get());
    }
  }

  void add_connections(const Graph &g) {
    for (auto &pair : g.get_port_inputs()) {
      std::string attrs = is_streaming(pair.second) ? "[color=red]" : "";
      line(port_str.at(pair.second) + ":e -> " + port_str.at(pair.first) + ":w" + attrs + ";");
    }
  }

  void start_block(const std::string &l) {
    line(l + " {");
    indent++;
  }

  void end_block() {
    indent--;
    always_assert(indent >= 0, "mismatching start_block/end_block");
    line("}");
  }

  void line(const std::string &l) { s << std::string(static_cast<size_t>(indent * 2), ' ') << l << "\n"; }

  static std::string element_name(const std::string &prefix, void *ptr) {
    return prefix + std::to_string(reinterpret_cast<std::uintptr_t>(ptr));
  }

  int indent = 0;
  std::ostream &s;
  bool recursive;
  std::map<PortPtr, std::string> port_str;
};

void graph_to_dot(std::ostream &s, const Graph &g, bool recursive) { DotFormatter(s, recursive).format(g); }

}  // namespace eat::utilities
