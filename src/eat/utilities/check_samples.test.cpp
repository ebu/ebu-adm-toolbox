#include "check_samples.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <random>
#include <vector>

#include "eat/process/block.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace eat::utilities;
using namespace Catch::Matchers;

TEST_CASE("check_samples") {
  size_t n_channels = 2;
  size_t n_samples = 1000;
  std::vector<float> samples(n_channels * n_samples);

  std::random_device rd;
  auto random_engine = std::default_random_engine{rd()};
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

  for (auto &sample : samples) sample = dist(random_engine);

  auto get_block = [&](size_t &start, size_t size) {
    size_t end = start + size;
    REQUIRE(end <= n_samples);

    auto block = std::make_shared<InterleavedSampleBlock>(
        std::vector<float>{samples.begin() + start * n_channels, samples.begin() + end * n_channels},
        BlockDescription{size, n_channels, 48000});
    start += size;

    return block;
  };

  size_t reference_start = 0;
  size_t test_start = 0;

  auto get_reference_block = [&](size_t size) { return get_block(reference_start, size); };
  auto get_test_block = [&](size_t size) { return get_block(test_start, size); };

  auto error_cb = [](const std::string &error) { throw std::runtime_error(error); };

  ProcessPtr generic_process = make_check_samples("check_samples", 1e-6f, 1e-6f, error_cb);
  auto process = std::dynamic_pointer_cast<StreamingAtomicProcess>(generic_process);
  REQUIRE(process);
  auto ref_port = process->get_in_port<StreamPort<InterleavedBlockPtr>>("in_samples_ref");
  auto test_port = process->get_in_port<StreamPort<InterleavedBlockPtr>>("in_samples_test");

  process->initialise();

  SECTION("basic") {
    ref_port->push(get_reference_block(500));
    test_port->push(get_test_block(500));
    process->process();

    ref_port->push(get_reference_block(500));
    test_port->push(get_test_block(500));
    ref_port->close();
    test_port->close();
    process->process();

    process->finalise();
  }

  SECTION("different samples") {
    ref_port->push(get_reference_block(500));
    auto test_block = get_test_block(500);
    test_block->sample(1, 5) += 0.1;
    test_port->push(test_block);
    CHECK_THROWS_WITH(process->process(), StartsWith("difference at sample"));
  }

  SECTION("different block sizes") {
    ref_port->push(get_reference_block(500));
    test_port->push(get_test_block(300));
    process->process();

    ref_port->push(get_reference_block(500));
    ref_port->close();
    test_port->push(get_test_block(300));
    process->process();

    test_port->push(get_test_block(400));
    test_port->close();
    process->process();

    process->finalise();
  }

  SECTION("different block sizes and samples") {
    ref_port->push(get_reference_block(500));
    test_port->push(get_test_block(300));
    process->process();

    ref_port->push(get_reference_block(500));
    ref_port->close();
    test_port->push(get_test_block(300));
    process->process();

    auto test_block = get_test_block(400);
    test_block->sample(1, 5) += 0.1;
    test_port->push(test_block);
    test_port->close();
    CHECK_THROWS_WITH(process->process(), StartsWith("difference at sample"));
  }

  SECTION("different lengths") {
    ref_port->push(get_reference_block(500));
    test_port->push(get_test_block(400));
    process->process();

    ref_port->push(get_reference_block(500));
    test_port->push(get_test_block(400));
    ref_port->close();
    test_port->close();
    process->process();

    CHECK_THROWS_WITH(process->finalise(), Equals("reference and test lengths differ: reference=1000, test=800"));
  }

  SECTION("different channel counts") {
    ref_port->push(get_reference_block(500));
    test_port->push(std::make_shared<InterleavedSampleBlock>(BlockDescription{500, 1, 48000}));
    CHECK_THROWS_WITH(process->process(), Equals("unexpected channel count: got 1 but expected 2"));
  }

  SECTION("different sample rates") {
    ref_port->push(get_reference_block(500));
    test_port->push(std::make_shared<InterleavedSampleBlock>(BlockDescription{500, 2, 48001}));
    CHECK_THROWS_WITH(process->process(), Equals("unexpected sample rate: got 48001 but expected 48000"));
  }

  SECTION("zero size blocks") {
    ref_port->push(get_reference_block(0));
    test_port->push(get_test_block(500));
    process->process();

    ref_port->push(get_reference_block(501));
    test_port->push(get_test_block(0));
    ref_port->close();
    test_port->close();
    process->process();

    CHECK_THROWS_WITH(process->finalise(), Equals("reference and test lengths differ: reference=501, test=500"));
  }
}
