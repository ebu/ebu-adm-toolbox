#include <cmath>
#include <cstdlib>
#include <iostream>

#include "eat/framework/evaluate.hpp"
#include "eat/process/adm_bw64.hpp"
#include "eat/process/loudness.hpp"
#include "eat/process/misc.hpp"
#include "eat/utilities/to_dot.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace eat::utilities;

int main(int argc, char **argv) {
  if (argc < 3) {
    std::cout << "add loudness information to audioProgrammes in an ADM BW64 file\n";
    std::cout << "usage: " << (argc ? argv[0] : "PROGRAM") << " in.wav out.wav\n";
    return 1;
  }

  std::string in_path = argv[1];
  std::string out_path = argv[2];

  Graph g;

  // equivalent to example_configs/measure_all_loudness.json
  auto reader = g.register_process(make_read_adm_bw64("reader", in_path, 1024));
  auto add_block_rtimes = g.register_process(make_add_block_rtimes("add_block_rtimes"));
  auto measure_loudness = g.register_process(make_update_all_programme_loudnesses("measure_loudness"));
  auto writer = g.register_process(make_write_adm_bw64("writer", out_path));

  g.connect(reader->get_out_port("out_samples"), writer->get_in_port("in_samples"));
  g.connect(reader->get_out_port("out_samples"), measure_loudness->get_in_port("in_samples"));

  g.connect(reader->get_out_port("out_axml"), add_block_rtimes->get_in_port("in_axml"));
  g.connect(add_block_rtimes->get_out_port("out_axml"), measure_loudness->get_in_port("in_axml"));
  g.connect(measure_loudness->get_out_port("out_axml"), writer->get_in_port("in_axml"));

  Plan p = plan(g);

  if (getenv("SHOW_GRAPH") != nullptr) graph_to_dot(std::cout, g, getenv("SHOW_RECURSIVE") != nullptr);
  if (getenv("SHOW_FLAT") != nullptr) graph_to_dot(std::cout, flatten(g));
  if (getenv("SHOW_PLAN_GRAPH") != nullptr) graph_to_dot(std::cout, p.graph());

  p.run();
}
