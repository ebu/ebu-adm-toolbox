#include "eat/render/render.hpp"

#include <catch2/catch_test_macros.hpp>
#include <ear/bs2051.hpp>

#include "../utilities/check_samples.hpp"
#include "../utilities/test_files.hpp"
#include "eat/framework/evaluate.hpp"
#include "eat/framework/process.hpp"
#include "eat/process/adm_bw64.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace eat::render;
using namespace eat::utilities;

// render in_fname and check the output is the same as reference_fname
//
// if rendered_fname is specified, the rendered samples will be written to it
void run_test(const std::string &in_fname, const std::string &reference_fname, const std::string &rendered_fname = "") {
  Graph g;
  const size_t block_size = 1024;

  auto read_adm = g.register_process(make_read_adm_bw64("read_adm", in_fname, block_size));
  auto read_reference = g.register_process(make_read_bw64("read_audio", reference_fname, block_size));

  auto layout = ear::getLayout("0+5+0");
  auto renderer = make_render("renderer", layout, block_size);
  g.register_process(renderer);

  bool has_error = false;
  auto error_cb = [&has_error](const std::string &error) {
    UNSCOPED_INFO(error);
    has_error = true;
  };

  auto check = make_check_samples("check", 1e-6f, 1e-6f, error_cb);
  g.register_process(check);

  g.connect(read_adm->get_out_port("out_samples"), renderer->get_in_port("in_samples"));
  g.connect(read_adm->get_out_port("out_axml"), renderer->get_in_port("in_axml"));

  g.connect(renderer->get_out_port("out_samples"), check->get_in_port("in_samples_test"));
  g.connect(read_reference->get_out_port("out_samples"), check->get_in_port("in_samples_ref"));

  if (rendered_fname.size()) {
    auto write = g.register_process(make_write_bw64("write_rendered", rendered_fname));
    g.connect(renderer->get_out_port("out_samples"), write->get_in_port("in_samples"));
  }

  evaluate(g);
  REQUIRE(!has_error);
}

TEST_CASE("render and check") {
  // run a single one of these with the -c flag, like:
  // ./build/test_eat "render and check" -c object_delay

  // clang-format off
  const std::vector<std::string> samples = {
    "channel_routing",
    "diffuse",
    "test_bwf",
    "interpolation_length",
    "object_delay",
    "silent_before_after_ds",
    "silent_before_after",
    "timing_on_object",
    "zero_size",
    "hoa_routing",
    "hoa_object_delay",
    "hoa_timing_on_object",
  };
  // clang-format on

  for (const std::string &sample : samples) {
    const std::string in_fname = test_file_path("render/" + sample + ".wav");
    const std::string reference_fname = test_file_path("render/" + sample + "_0_5_0.wav");
    std::string rendered_fname;
    // to write rendered results for comparison:
    // rendered_fname = test_file_path("render/" + sample + "_0_5_0_r.wav");

    SECTION(sample) { run_test(in_fname, reference_fname, rendered_fname); }
  }
}
