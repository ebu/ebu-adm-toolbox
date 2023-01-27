//
// Created by Richard Bailey on 13/05/2022.
//
#include "eat/process/silence_detect.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "eat/framework/evaluate.hpp"
#include "eat/framework/utility_processes.hpp"
#include "eat/process/block.hpp"

namespace fw = eat::framework;
namespace process = eat::process;

TEST_CASE("Streaming mono audio") {
  fw::Graph g;
  const std::size_t samples = 16;
  const std::size_t channels = 1;

  std::vector<float> input;
  for (auto i = 0; i != samples; ++i) {
    input.push_back(static_cast<float>(i));
  }

  auto source = g.add_process<process::InterleavedStreamingAudioSource>(
      "source", input, process::BlockDescription{samples, channels, 44100});
  auto sink = g.add_process<process::InterleavedStreamingAudioSink>("sink");
  g.connect(source->get_out_port("out_samples"), sink->get_in_port("in_samples"));

  fw::Plan p = plan(g);
  p.run();

  auto &result = sink->get();
  auto outCount = result.size();
  REQUIRE(outCount == input.size());
  for (auto i = 0u; i != outCount; ++i) {
    INFO(i);
    REQUIRE(result[i] == Catch::Approx(input[i]));
  }
}

TEST_CASE("Streaming stereo audio") {
  fw::Graph g;
  const std::size_t samples = 16;
  const std::size_t channels = 2;
  std::vector<float> input;
  for (auto i = 0; i != samples; ++i) {
    input.push_back(1.0f);
    input.push_back(-1.0f);
  }

  auto source = g.add_process<process::InterleavedStreamingAudioSource>(
      "source", input, process::BlockDescription{samples, channels, 44100});
  auto sink = g.add_process<process::InterleavedStreamingAudioSink>("sink");

  g.connect(source->get_out_port("out_samples"), sink->get_in_port("in_samples"));
  fw::Plan p = plan(g);
  p.run();

  auto &result = sink->get();
  auto outCount = result.size();
  REQUIRE(outCount == input.size());
  for (auto i = 0u; i != outCount; ++i) {
    INFO(i);
    REQUIRE(result[i] == Catch::Approx(input[i]));
  }
}

namespace {
struct DetectorHarness {
  using result_t = std::vector<process::AudioInterval>;
  using sink_t = fw::DataSink<result_t>;
  using source_t = process::InterleavedStreamingAudioSource;
  explicit DetectorHarness(std::vector<float> input, process::BlockDescription frame_description = {16, 1, 44100},
                           process::SilenceDetectionConfig silence_config = {10, -95.0})
      : source{graph.add_process<source_t>("source", std::move(input), frame_description)},
        data_sink{graph.add_process<sink_t>("data_sink")},
        detector{graph.add_process<process::SilenceDetector>("silence_detector", process::SilenceDetectionConfig{})} {
    graph.connect(detector->get_out_port("out_intervals"), data_sink->get_in_port("in"));
    graph.connect(source->get_out_port("out_samples"), detector->get_in_port("in_samples"));
  }

  result_t run() {
    auto p = plan(graph);
    p.run();
    return data_sink->get_value();
  }

  fw::Graph graph;
  std::shared_ptr<process::InterleavedStreamingAudioSource> source;
  std::shared_ptr<sink_t> data_sink;
  std::shared_ptr<process::SilenceDetector> detector;
};

}  // namespace

TEST_CASE("Detect mono silence over single block") {
  auto numSamples{16ul};
  DetectorHarness h(std::vector<float>(numSamples, 0.0f));
  auto result = h.run();
  REQUIRE(result.size() == 1);
  auto const interval = result.front();
  REQUIRE(interval.start == 0);
  REQUIRE(interval.length == numSamples);
}

TEST_CASE("Detect mono silence over two blocks") {
  auto numSamples{16ul};
  DetectorHarness h(std::vector<float>(numSamples, 0.0f), {numSamples / 2, 1, 44100});
  auto result = h.run();
  REQUIRE(result.size() == 1);
  auto const interval = result.front();
  REQUIRE(interval.start == 0);
  REQUIRE(interval.length == numSamples);
}

TEST_CASE("Detecting silence in audio with constant signal results in no intervals") {
  auto numSamples{16ul};
  std::vector<float> input;
  input.reserve(numSamples);
  for (auto i = 0; i != numSamples; ++i) {
    input.push_back(static_cast<float>(std::pow(-1.0, i)));
  }
  DetectorHarness h(std::move(input), {numSamples, 1, 44100});

  auto result = h.run();
  REQUIRE(result.empty());
}

TEST_CASE("Detecting silent block between signal") {
  auto numSamples{16ul};
  std::vector<float> input;
  input.reserve(numSamples);
  for (auto i = 0; i != numSamples; ++i) {
    input.push_back(static_cast<float>(std::pow(-1.0, i)));
  }
  std::fill(input.begin() + 2, input.end() - 2, 0.0f);

  DetectorHarness h(std::move(input), {numSamples, 1, 44100});

  auto result = h.run();
  REQUIRE(result.size() == 1);
  auto interval = result.front();
  REQUIRE(interval.length == 12);
  REQUIRE(interval.start == 2);
}

TEST_CASE("Silent block of too few samples not detected") {
  auto numSamples{16ul};
  std::vector<float> input;
  input.reserve(numSamples);
  for (auto i = 0; i != numSamples; ++i) {
    input.push_back(static_cast<float>(std::pow(-1.0, i)));
  }
  std::fill(input.begin() + 2, input.end() - 5, 0.0f);

  DetectorHarness h(std::move(input), {numSamples, 1, 44100});

  auto result = h.run();
  REQUIRE(result.empty());
}

TEST_CASE("Silent block of just enough samples not detected") {
  auto numSamples{16ul};
  std::vector<float> input;
  input.reserve(numSamples);
  for (auto i = 0; i != numSamples; ++i) {
    input.push_back(static_cast<float>(std::pow(-1.0, i)));
  }
  std::fill(input.begin() + 3, input.end() - 3, 0.0f);

  DetectorHarness h(std::move(input), {numSamples, 1, 44100});

  auto result = h.run();
  REQUIRE(result.size() == 1);
  auto interval = result.front();
  REQUIRE(interval.length == 10);
  REQUIRE(interval.start == 3);
}
