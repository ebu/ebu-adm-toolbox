#pragma once
#include "process.hpp"

namespace eat::framework {

Graph flatten(const Graph &g);

void validate(const Graph &g);

/// representation of a step within an execution plan
///
/// this doesn't do much more than std::function, but this is where we would add
/// progress callbacks
class ExecStep {
 public:
  virtual ~ExecStep() {}

  virtual void run() = 0;

  virtual std::string description() = 0;
};
using ExecStepPtr = std::shared_ptr<ExecStep>;

/// a plan for evaluating a graph
class Plan {
 public:
  Plan(Graph graph, std::vector<ExecStepPtr> steps) : graph_(std::move(graph)), steps_(std::move(steps)) {}

  /// get the actual graph that will be evaluated
  ///
  /// this is useful for debugging to see the changes made by the planner
  const Graph &graph() const { return graph_; }

  /// get the steps in the plan
  const std::vector<ExecStepPtr> &steps() const { return steps_; }

  /// run all steps in the plan
  void run() {
    for (auto &step : steps_) step->run();
  }

 private:
  Graph graph_;
  std::vector<ExecStepPtr> steps_;
};

/// plan the evaluation of graph
Plan plan(const Graph &g);

/// evaluate a graph; equivalent to plan(g).run()
void evaluate(const Graph &g);

/// run a plan while printing progress updates to the terminal
void run_with_progress(const Plan &p);

}  // namespace eat::framework
