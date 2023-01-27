//
// Created by Richard Bailey on 13/05/2022.
//
#pragma once
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "eat/framework/exceptions.hpp"
#include "eat/framework/process.hpp"
#include "eat/framework/value_ptr.hpp"

namespace eat::process {

/// description of a block of samples (independent of storage format)
struct BlockDescription {
  /// number of samples
  std::size_t sample_count;
  /// number of channels
  std::size_t channel_count;
  /// sample rate in Hz
  unsigned int sample_rate;
};

/// a block of samples in which samples for each channel are interleaved
///
/// see also @ref PlanarSampleBlock, which is equivalent but with planar
/// (non-interleaved) channels
class InterleavedSampleBlock {
 public:
  /// construct with existing samples, which must have a size of sample_count * channel_count
  InterleavedSampleBlock(std::vector<float> samples, BlockDescription blockInfo)
      : samples_(std::move(samples)), info_{blockInfo} {
    framework::always_assert(samples_.size() == info_.sample_count * info_.channel_count,
                             "wrong number of samples in SampleBlock");
  }

  /// construct with zero-valued  samples
  InterleavedSampleBlock(BlockDescription blockInfo)
      : samples_(blockInfo.sample_count * blockInfo.channel_count), info_{blockInfo} {}

  /// get the block description (sample and channel count, sample rate)
  [[nodiscard]] BlockDescription const &info() const { return info_; }

  /// access a single sample
  [[nodiscard]] float sample(size_t channel, size_t sample) const {
    assert(channel < info_.channel_count);
    assert(sample < info_.sample_count);

    return samples_[info_.channel_count * sample + channel];
  };
  /// access a single sample
  float &sample(size_t channel, size_t sample) {
    assert(channel < info_.channel_count);
    assert(sample < info_.sample_count);

    return samples_[info_.channel_count * sample + channel];
  }

  /// access the sample data
  ///
  /// sample s of channel c is at data()[s * info().channel_count + c]
  [[nodiscard]] const float *data() const { return samples_.data(); }
  /// access the sample data
  ///
  /// sample s of channel c is at data()[s * info().channel_count + c]
  float *data() { return samples_.data(); }

 private:
  std::vector<float> samples_;
  BlockDescription info_;
};

/// pointer to an interleaved sample block
using InterleavedBlockPtr = framework::ValuePtr<InterleavedSampleBlock>;

/// a block of planar samples
///
/// see also @ref InterleavedSampleBlock, which is equivalent but with
/// interleaved channels
class PlanarSampleBlock {
 public:
  /// construct with existing samples, which must have a size of sample_count * channel_count
  PlanarSampleBlock(std::vector<float> samples, BlockDescription blockInfo)
      : samples_(std::move(samples)), info_{blockInfo} {
    framework::always_assert(samples_.size() == info_.sample_count * info_.channel_count,
                             "wrong number of samples in SampleBlock");
  }
  /// construct with zero-valued  samples
  PlanarSampleBlock(BlockDescription blockInfo)
      : samples_(blockInfo.sample_count * blockInfo.channel_count), info_{blockInfo} {}

  /// get the block description (sample and channel count, sample rate)
  [[nodiscard]] BlockDescription const &info() const { return info_; }

  /// access a single sample
  [[nodiscard]] float sample(size_t channel, size_t sample) const {
    return const_cast<PlanarSampleBlock *>(this)->sample(channel, sample);
  };
  /// access a single sample
  float &sample(size_t channel, size_t sample) {
    assert(channel < info_.channel_count);
    assert(sample < info_.sample_count);

    return samples_[info_.sample_count * channel + sample];
  }

  /// access the sample data
  ///
  /// sample s of channel c is at data()[c * info().sample_count + s]
  [[nodiscard]] const float *data() const { return samples_.data(); }
  /// access the sample data
  ///
  /// sample s of channel c is at data()[c * info().sample_count + s]
  float *data() { return samples_.data(); }

 private:
  std::vector<float> samples_;
  BlockDescription info_;
};

/// pointer to a planar sample block
using PlanarBlockPtr = framework::ValuePtr<PlanarSampleBlock>;

/// a process which produces InterleavedSampleBlock objects from a buffer
/// provided at initialisation
class InterleavedStreamingAudioSource : public framework::StreamingAtomicProcess {
 public:
  /// construct with some samples
  ///
  /// @param samples interleaved samples
  /// @param blockInfo shape of the produced blocks. channel_count and
  ///     sample_rate will be kept as is, but sample_count is treated as the maximum
  ///     number of samples to produce in one block (if the number of samples is not
  ///     divisible by blockInfo.sample_count)
  InterleavedStreamingAudioSource(std::string const &name, std::vector<float> samples, BlockDescription blockInfo)
      : StreamingAtomicProcess(name),
        source(std::move(samples)),
        block_info(blockInfo),
        position(source.begin()),
        out(add_out_port<framework::StreamPort<InterleavedBlockPtr>>("out_samples")) {
    framework::always_assert(samples.size() % blockInfo.channel_count == 0,
                             "number of samples must be divisible by channel count");
  }

  void process() override {
    if (auto samplesLeft = static_cast<std::size_t>(source.end() - position); samplesLeft > 0) {
      auto framesLeft = samplesLeft / block_info.channel_count;
      auto nextBlockSize = std::min(block_info.sample_count, framesLeft);
      auto nextPosition = position + static_cast<std::ptrdiff_t>(nextBlockSize * block_info.channel_count);
      auto nextInfo = block_info;
      nextInfo.sample_count = nextBlockSize;
      auto block = std::make_shared<InterleavedSampleBlock>(std::vector<float>(position, nextPosition), nextInfo);
      out->push(block);
      position = nextPosition;
    } else {
      out->close();
    }
  }

 private:
  std::vector<float> source;
  BlockDescription block_info;
  std::vector<float>::const_iterator position;
  framework::StreamPortPtr<InterleavedBlockPtr> out;
};

/// a sink for InterleavedSampleBlock which stores the samples, to be retrieved
/// after a processing graph has completed
class InterleavedStreamingAudioSink : public framework::StreamingAtomicProcess {
 public:
  explicit InterleavedStreamingAudioSink(std::string const &name)
      : StreamingAtomicProcess(name), in(add_in_port<framework::StreamPort<InterleavedBlockPtr>>("in_samples")) {}

  /// access the vector of samples
  std::vector<float> const &get() { return samples; }

  /// get the samples as an InterleavedSampleBlock
  InterleavedSampleBlock get_block() { return {samples, info}; }

  void initialise() override { has_input = false; }

  void process() override {
    while (in->available()) {
      auto block = in->pop().read();
      auto &block_info = block->info();

      if (has_input) {
        framework::always_assert(info.channel_count == block_info.channel_count, "channel count changed mid-stream");
        framework::always_assert(info.sample_rate == block_info.sample_rate, "sample rate changed mid-stream");
      } else {
        has_input = true;
        info.channel_count = block_info.channel_count;
        info.sample_rate = block_info.sample_rate;
        info.sample_count = 0;
      }

      info.sample_count += block_info.sample_count;

      for (auto sample_number = 0ul; sample_number != block_info.sample_count; ++sample_number) {
        for (auto channel_number = 0ul; channel_number != block_info.channel_count; ++channel_number) {
          samples.push_back(block->sample(channel_number, sample_number));
        }
      }
    }
  }

 private:
  framework::StreamPortPtr<InterleavedBlockPtr> in;

  bool has_input = false;
  BlockDescription info;
  std::vector<float> samples;
};
}  // namespace eat::process

// overloads for buffering InterleavedSampleBlock objects to disk
namespace eat::framework {

template <>
ProcessPtr MakeBuffer<process::InterleavedBlockPtr>::get_buffer_reader(const std::string &name);

template <>
ProcessPtr MakeBuffer<process::InterleavedBlockPtr>::get_buffer_writer(const std::string &name);

}  // namespace eat::framework
