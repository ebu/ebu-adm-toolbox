#include "eat/framework/evaluate.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "eat/framework/exceptions.hpp"
#include "exec_steps.hpp"
#include "utilities.hpp"

namespace eat::framework {

/// visitor for the composite graph tree structure
class GraphVisitor {
 public:
  virtual void visit_graph(const Graph &) {}
  virtual void visit_composite(const CompositeProcessPtr &) {}
  virtual void visit_streaming(const StreamingAtomicProcessPtr &) {}
  virtual void visit_functional(const FunctionalAtomicProcessPtr &) {}

  /// calls visit_* once for each process/graph in g, recursively based on the
  /// concrete type
  void run(const Graph &g) {
    visit_graph(g);
    run_subprocesses(g);
  }

 private:
  void run_subprocesses(const Graph &g) {
    for (auto &process : g.get_processes()) {
      auto composite_process = std::dynamic_pointer_cast<CompositeProcess>(process);
      if (composite_process) {
        visit_composite(composite_process);
        run_subprocesses(*composite_process);
      }

      auto functional_process = std::dynamic_pointer_cast<FunctionalAtomicProcess>(process);
      if (functional_process) visit_functional(functional_process);

      auto streaming_process = std::dynamic_pointer_cast<StreamingAtomicProcess>(process);
      if (streaming_process) visit_streaming(streaming_process);
    }
  }
};

/// recursively add processes in g to new_graph, and accumulate connections into port_inputs
static void do_flatten(Graph &new_graph, std::map<PortPtr, PortPtr> &port_inputs, const Graph &g) {
  for (auto &[downstream, upstream] : g.get_port_inputs()) {
    port_inputs[downstream] = upstream;
  }

  for (const ProcessPtr &process : g.get_processes()) {
    if (std::dynamic_pointer_cast<AtomicProcess>(process)) new_graph.register_process(process);

    GraphPtr subgraph = std::dynamic_pointer_cast<Graph>(process);
    if (subgraph) do_flatten(new_graph, port_inputs, *subgraph);
  }
}

Graph flatten(const Graph &g) {
  std::map<PortPtr, PortPtr> port_inputs;
  Graph new_graph;
  do_flatten(new_graph, port_inputs, g);

  // connect the processes together. for each input port of each process, trace
  // back the connections in upstream_port to find the corresponding output
  for (auto &process : new_graph.get_processes()) {
    for (auto &[name, port] : process->get_in_port_map()) {
      PortPtr upstream_port = port;
      while (true) {
        auto it = port_inputs.find(upstream_port);
        if (it == port_inputs.end()) break;
        upstream_port = it->second;
      }
      always_assert(port != upstream_port, "all ports must be connected");
      new_graph.connect(upstream_port, port);
    }
  }

  return new_graph;
}

/// check that all ports are connected
class ValidateAllConnected : public GraphVisitor {
  void visit_graph(const Graph &g) override { validate_graph(g, {}, {}); }

  void visit_composite(const CompositeProcessPtr &p) override {
    std::set<PortPtr> sources;
    std::set<PortPtr> sinks;
    populate_port_sets(*p, sources, sinks);

    validate_graph(*p, std::move(sources), std::move(sinks));
  }

  void validate_graph(const Graph &g, std::set<PortPtr> sources, std::set<PortPtr> sinks) {
    for (auto &process : g.get_processes()) populate_port_sets(*process, sinks, sources);

    for (auto &[downstream, upstream] : g.get_port_inputs()) {
      sinks.erase(downstream);
      sources.erase(upstream);
    }

    auto port_names = [](const std::set<PortPtr> &ports) {
      std::string names;
      for (auto &port : ports) names += " " + port->name();
      return names;
    };

    if (sources.size() && sinks.size())
      throw std::runtime_error("found unconnected sources:" + port_names(sources) + " and sinks:" + port_names(sinks));
    else if (sources.size())
      throw std::runtime_error("found unconnected sources:" + port_names(sources));
    else if (sinks.size())
      throw std::runtime_error("found unconnected sinks:" + port_names(sinks));
  }

  void populate_port_sets(const Process &p, std::set<PortPtr> &inputs, std::set<PortPtr> &outputs) {
    for (auto &[name, port] : p.get_in_port_map()) inputs.insert(port);
    for (auto &[name, port] : p.get_out_port_map()) outputs.insert(port);
  }
};

/// check that functional processes do not have streaming ports
/// TODO: check on port addition intead
class ValidateStreaming : public GraphVisitor {
  void visit_functional(const FunctionalAtomicProcessPtr &p) {
    for (auto &[name, port] : p->get_port_map())
      if (std::dynamic_pointer_cast<StreamPortBase>(port))
        throw std::runtime_error("functional process has streaming port");
  }
};

void validate(const Graph &g) {
  ValidateAllConnected{}.run(g);
  ValidateStreaming{}.run(g);

  // TODO
  // - processes are loop-free
}

// given that processes in ran_start have been ran find all streaming processes
// which could run, ignoring any processes that become active because of
// non-streaming connections from processes not in ran_start
//
// to_run_start is just the complement of ran_start, but generally this will be
// available
static std::set<ProcessPtr> runnable_streaming_processes(const Graph &g, const std::set<ProcessPtr> &ran_start,
                                                         const std::set<ProcessPtr> &to_run_start) {
  std::set<ProcessPtr> ran = ran_start;
  std::set<ProcessPtr> runnable;
  std::set<ProcessPtr> to_run = to_run_start;

  auto update_runnable = [&]() {
    runnable.clear();
    for (auto &process : to_run) {
      if (!is_streaming(process)) continue;

      auto connections = input_connections(g, process);
      bool all_inputs_ran = std::all_of(connections.begin(), connections.end(), [&](auto &connection) {
        // only consider processes that are not part of the possible
        // streaming subgraphs for non-streaming connections
        if (connection.is_streaming())
          return ran.find(connection.upstream_process) != ran.end();
        else
          return ran_start.find(connection.upstream_process) != ran_start.end();
      });

      if (all_inputs_ran) runnable.insert(process);
    }
  };

  update_runnable();
  always_assert(runnable.size(), "expected at least one runnable process");

  std::set<ProcessPtr> all_runnable;

  while (runnable.size()) {
    auto process = *runnable.begin();
    ran.insert(process);
    to_run.erase(process);

    all_runnable.insert(process);

    update_runnable();
  }

  return all_runnable;
}

// find processes connected by streaming connections, starting at 'start',
// considering only processes in 'processes'
static std::set<ProcessPtr> streaming_subgraph_from_set(const Graph &g, const std::set<ProcessPtr> &processes,
                                                        const ProcessPtr &start) {
  std::set<ProcessPtr> out = {};
  // processes yet to be visited
  std::vector<ProcessPtr> to_process = {start};

  while (to_process.size()) {
    auto p = to_process.back();
    to_process.pop_back();

    if (!out.insert(p).second) continue;

    for (auto &connection : output_connections(g, p)) {
      if (!connection.is_streaming()) continue;

      if (processes.find(connection.downstream_process) == processes.end()) continue;

      if (out.find(connection.downstream_process) == out.end()) to_process.push_back(connection.downstream_process);
    }

    for (auto &connection : input_connections(g, p)) {
      if (!connection.is_streaming()) continue;

      if (processes.find(connection.upstream_process) == processes.end()) continue;

      if (out.find(connection.upstream_process) == out.end()) to_process.push_back(connection.upstream_process);
    }
  }

  return out;
}

// find sets of processes connected by streaming connections, considering only
// processes in 'processes'
static std::set<std::set<ProcessPtr>> streaming_subgraphs_from_set(const Graph &g,
                                                                   const std::set<ProcessPtr> &processes) {
  std::set<std::set<ProcessPtr>> out;
  std::set<ProcessPtr> all_visited;

  for (auto &process : processes) {
    if (all_visited.find(process) == all_visited.end()) {
      auto subgraph = streaming_subgraph_from_set(g, processes, process);
      all_visited.insert(subgraph.begin(), subgraph.end());
      out.insert(std::move(subgraph));
    }
  }

  return out;
}

/// are all connections from processes in subgraph to other processes in the subgraph?
static bool is_complete_streaming_subgraph(const Graph &g, const std::set<ProcessPtr> &subgraph) {
  for (auto &process : subgraph) {
    for (auto &connection : output_connections(g, process))
      if (connection.is_streaming() && subgraph.find(connection.downstream_process) == subgraph.end()) return false;
    for (auto &connection : input_connections(g, process))
      if (connection.is_streaming() && subgraph.find(connection.upstream_process) == subgraph.end()) return false;
  }
  return true;
}

static std::vector<std::set<ProcessPtr>> subgraphs_in_order(const Graph &g, bool allow_split = false) {
  std::vector<std::set<ProcessPtr>> out;
  std::set<ProcessPtr> runnable;
  std::set<ProcessPtr> to_run(g.get_processes().begin(), g.get_processes().end());
  std::set<ProcessPtr> ran;

  auto update_runnable = [&]() {
    runnable.clear();
    for (auto &process : to_run) {
      auto connections = input_connections(g, process);
      bool all_inputs_ran = std::all_of(connections.begin(), connections.end(), [&](auto &connection) {
        return ran.find(connection.upstream_process) != ran.end();
      });

      if (all_inputs_ran) runnable.insert(process);
    }
  };

  update_runnable();

  while (to_run.size()) {
    always_assert(runnable.size(), "expected at least one runnable process");

    ProcessPtr process_to_run;
    for (auto &process : runnable) {
      if (!is_streaming(process)) {
        process_to_run = process;
        break;
      }
    }

    std::set<ProcessPtr> subgraph_to_run;
    // if there is a runnable non-streaming processes, handle it normally
    if (process_to_run)
      subgraph_to_run = {process_to_run};
    else {
      // otherwise, find connected streaming subgraphs whose non-streaming
      // inputs have all been ran. to do this:
      //
      // - find all streaming processes that could possibly run, assuming that
      //   non-streaming connections are ignored (make list of runnable and
      //   select one repeatedly, as above)
      // - split this into connected sub-graphs
      // - classify these as complete (no external streaming connections) or incomplete
      // - pick a complete one if possible. otherwise if allow_split, pick an incomplete one
      std::set<ProcessPtr> runnable_streaming = runnable_streaming_processes(g, ran, to_run);
      always_assert(runnable_streaming.size(), "found no runnable streaming processes");
      auto subgraphs = streaming_subgraphs_from_set(g, runnable_streaming);
      always_assert(runnable_streaming.size(), "found no streaming subgraphs");

      for (auto &subgraph : subgraphs)
        if (is_complete_streaming_subgraph(g, subgraph)) {
          subgraph_to_run = subgraph;
          break;
        }

      if (!subgraph_to_run.size() && allow_split) subgraph_to_run = *subgraphs.begin();

      always_assert(subgraph_to_run.size(),
                    "found no complete streaming subgraph. "
                    "this is either a logic error, or buffers were not specified correctly");
    }

    for (auto &process : subgraph_to_run) {
      ran.insert(process);
      to_run.erase(process);
    }
    out.push_back(std::move(subgraph_to_run));

    update_runnable();
  }

  return out;
}

// adds ExecCopyData step to plan to copy/move data to all connected ports
static void add_data_copy_to_plan(const Graph &g, std::vector<ExecStepPtr> &plan, const DataPortBasePtr &port) {
  std::vector<DataPortBasePtr> ports;
  for (auto &connected_port : connected_ports(g, port)) {
    auto connected_port_data = checked_dynamic_pointer_cast<DataPortBase>(connected_port);
    ports.push_back(connected_port_data);
  }

  if (ports.size()) plan.push_back(std::make_shared<ExecCopyData>(port, ports));
}

/// given some subgraphs to run, add extra processes to eliminate streaming
/// connections between subgraphs
///
/// returns a new graph with the new connections and processes, which may need flattening
static Graph augment_subgraphs(const Graph &g, const std::vector<std::set<ProcessPtr>> &subgraphs) {
  Graph new_g;
  for (auto &process : g.get_processes()) new_g.register_process(process);

  // all connections in the new graph, kept separately so that we can remove them
  std::map<PortPtr, PortPtr> new_connections = g.get_port_inputs();

  auto find_subgraph = [&](const ProcessPtr &process) {
    for (size_t i = 0; i < subgraphs.size(); i++) {
      auto &subgraph = subgraphs.at(i);
      if (subgraph.find(process) != subgraph.end()) return i;
    }

    throw AssertionError("could not find subgraph for process");
  };

  for (auto &subgraph : subgraphs)
    for (auto &process : subgraph) {
      for (auto &[out_port_name, out_port] : process->get_out_port_map()) {
        // get connections from this port to other subgraphs (by index)
        std::map<size_t, std::vector<Connection>> connections_by_subgraph;
        for (auto &connection : output_connections(g, process)) {
          if (connection.upstream_port != out_port) continue;

          // non-streaming or connection in same subgraph -> handle as normal
          if (!connection.is_streaming() || subgraph.find(connection.downstream_process) != subgraph.end()) continue;

          size_t other_subgraph = find_subgraph(connection.downstream_process);

          auto &connection_list =
              connections_by_subgraph.emplace(other_subgraph, std::vector<Connection>{}).first->second;
          connection_list.push_back(connection);

          // remove from connections, as these will be added below
          new_connections.erase(connection.downstream_port);
        }

        // if streaming connections to other subgraphs were found, introduce
        // converters between streaming and non-streaming
        if (connections_by_subgraph.size()) {
          auto out_port_stream = checked_dynamic_pointer_cast<StreamPortBase>(out_port);

          ProcessPtr writer = out_port_stream->get_buffer_writer("buffer writer");
          new_g.register_process(writer);

          new_g.connect(out_port, writer->get_in_port("in"));

          for (auto &pair : connections_by_subgraph) {
            auto &connections = pair.second;

            ProcessPtr reader = out_port_stream->get_buffer_reader("buffer reader");
            new_g.register_process(reader);
            new_g.connect(writer->get_out_port("out"), reader->get_in_port("in"));

            for (auto &connection : connections) new_g.connect(reader->get_out_port("out"), connection.downstream_port);
          }
        }
      }
    }

  for (auto &connection : new_connections) new_g.connect(connection.second, connection.first);

  return new_g;
}

// within each subgraph, check that all streaming connections are internal, and all non-streaming connections
// are external
static void check_subgraph_connections(const Graph &g, const std::vector<std::set<ProcessPtr>> &subgraphs) {
  for (auto &subgraph : subgraphs)
    for (auto &process : subgraph)
      for (auto &connection : output_connections(g, process))
        if (connection.is_streaming())
          always_assert(subgraph.find(connection.downstream_process) != subgraph.end(),
                        "found streaming connection out of subgraph");
        else
          always_assert(subgraph.find(connection.downstream_process) == subgraph.end(),
                        "found non-streaming connection inside subgraph");
}

Plan plan(const Graph &g) {
  validate(g);
  Graph flat = flatten(g);

  std::vector<std::set<ProcessPtr>> subgraphs = subgraphs_in_order(flat, /* allow_split = */ true);

  flat = flatten(augment_subgraphs(flat, subgraphs));
  subgraphs = subgraphs_in_order(flat);

  check_subgraph_connections(flat, subgraphs);

  std::vector<ExecStepPtr> plan;

  for (auto &subgraph : subgraphs) {
    if (!is_streaming(*subgraph.begin())) {
      always_assert(subgraph.size() == 1, "non-streaming subgraphs should only have one process");
      const ProcessPtr &process = *subgraph.begin();

      auto atomic_process = checked_dynamic_pointer_cast<FunctionalAtomicProcess>(process);
      plan.push_back(std::make_shared<ExecFunctional>(atomic_process));
    } else {
      plan.push_back(std::make_shared<ExecStreamingSubgraph>(flat, subgraph));
    }

    // add output data copies
    for (auto &process : subgraph)
      for (auto &[port_name, port] : process->get_out_port_map()) {
        auto data_port = std::dynamic_pointer_cast<DataPortBase>(port);
        if (data_port) add_data_copy_to_plan(flat, plan, data_port);
      }
  }

  return {std::move(flat), std::move(plan)};
}

void evaluate(const Graph &g) {
  Plan p = plan(g);
  p.run();
}

}  // namespace eat::framework
