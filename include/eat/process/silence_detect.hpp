//
// Created by Richard Bailey on 13/05/2022.
//
#pragma once
#include <cmath>
#include <cstdint>
#include <numbers>

#include "eat/framework/process.hpp"
#include "eat/process/block.hpp"

namespace eat::process {
namespace detail {

inline double db_to_peak_amp(double dB) { return std::pow(10.0, dB / 20.0); }
inline double peak_amp_to_db(double amp) { return 20.0 * std::log10(amp); }

}  // namespace detail

struct AudioInterval {
  std::size_t start{0};
  std::size_t length{0};
};

struct SilenceDetectionConfig {
  /**
   * Number of consecutive samples that must test below the threshold to register as silence
   */
  std::size_t minimum_length{10};
  /**
   * Value in dB that will be used as a threshold for detecting silence
   */
  double threshold{-95.0};
};

class SilenceStatus {
 public:
  explicit SilenceStatus(SilenceDetectionConfig silence_config)
      : config(silence_config),
        squared_threshold(detail::db_to_peak_amp(config.threshold) * detail::db_to_peak_amp(config.threshold)) {}

  void process(InterleavedSampleBlock &block, std::size_t sample_number) {
    complete_interval = false;
    auto channelCount = block.info().channel_count;
    bool silent = channelCount > 0;
    for (std::size_t channel = 0; channel != channelCount; ++channel) {
      auto sample = block.sample(channel, sample_number);
      silent = silent && (sample * sample < squared_threshold);
      if (silent) {
        silence();
      } else {
        signal();
      }
      ++total;
    }
  }

  [[nodiscard]] bool ready() const { return complete_interval; }

  [[nodiscard]] AudioInterval getInterval() const {
    if (!ready()) {
      throw std::runtime_error("Interval requested when none was ready");
    }
    return interval;
  }

  void finish() { signal(); }

 private:
  void silence() {
    if (zero_count == 0) {
      interval.start = total;
      interval.length = 0;
    }
    ++zero_count;
  }

  void signal() {
    if (zero_count >= config.minimum_length) {
      interval.length = zero_count;
      complete_interval = true;
    }
    zero_count = 0;
  }

  SilenceDetectionConfig config;
  AudioInterval interval;
  bool complete_interval{false};

  std::size_t total{0};
  std::size_t zero_count{0};
  double squared_threshold;
};

class SilenceDetector : public framework::StreamingAtomicProcess {
 public:
  explicit SilenceDetector(std::string const &name, SilenceDetectionConfig config = {})
      : StreamingAtomicProcess{name},
        in_samples(add_in_port<framework::StreamPort<InterleavedBlockPtr>>("in_samples")),
        out_intervals(add_out_port<framework::DataPort<std::vector<AudioInterval>>>("out_intervals")),
        status_config{config},
        status{status_config} {}

  void initialise() override { status = SilenceStatus{status_config}; }

  void process() override {
    while (in_samples->available()) {
      auto samples = in_samples->pop().move_or_copy();
      auto &info = samples->info();
      for (std::size_t sample = 0; sample != info.sample_count; ++sample) {
        status.process(*samples, sample);
        add_interval_if_ready();
      }
    }
  };

  void finalise() override {
    status.finish();
    add_interval_if_ready();
    out_intervals->set_value(intervals);
  }

 private:
  void add_interval_if_ready() {
    if (status.ready()) {
      intervals.push_back(status.getInterval());
    }
  }

  framework::StreamPortPtr<InterleavedBlockPtr> in_samples;
  framework::DataPortPtr<std::vector<AudioInterval>> out_intervals;
  std::vector<AudioInterval> intervals;
  SilenceDetectionConfig status_config;
  SilenceStatus status;
};
}  // namespace eat::process
