#include "../utilities/progress.hpp"
#include "eat/framework/evaluate.hpp"
#include "exec_steps.hpp"

using namespace eat::utilities;

namespace eat::framework {

/// run each of the steps in sequence
/// - the overall progress is based on the step index and the number of steps
/// - is a step is an ExecStreamingSubgraph, run the initialise/run/finalise
///   ourselves so that we can update progress between each loop. each process
///   within the streaming subgraph can optionally report its progress (if
///   it's known, for example when reading a file), and these are averaged to
///   find the progress within this step
///
/// see RefreshWindow for a description of the actual update mechanism
void run_with_progress(const Plan &p) {
  auto &steps = p.steps();
  const int width = 50;

  RefreshWindow window;
  std::string msg;

  for (size_t step_i = 0; step_i < steps.size(); step_i++) {
    auto &step = steps[step_i];

    float overall_progress = static_cast<float>(step_i) / static_cast<float>(steps.size());

    if (auto streaming_step = std::dynamic_pointer_cast<ExecStreamingSubgraph>(step)) {
      std::string current_task = streaming_step->description();

      format_progress(msg, width, overall_progress, current_task, 0.0f);
      window.print(msg);

      streaming_step->run_initialise();

      do {
        streaming_step->run_run();

        auto progress = streaming_step->get_progress();
        format_progress(msg, width, overall_progress, current_task, progress.value_or(0.0f));
        window.print(msg);
      } while (streaming_step->runnable());

      streaming_step->run_finalise();
    } else {
      format_progress(msg, width, overall_progress, step->description(), 0.0f);
      window.print(msg);

      step->run();
    }
  }

  format_progress(msg, width, 1.0f, "done", 1.0f);
  window.print(msg);
  std::cout << "\n";
}

}  // namespace eat::framework
