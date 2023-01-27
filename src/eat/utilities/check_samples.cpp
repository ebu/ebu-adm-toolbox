#include "check_samples.hpp"

#include <functional>
#include <sstream>
#include <vector>

#include "eat/process/block.hpp"

using namespace eat::framework;
using namespace eat::process;

namespace eat::utilities {

/// a utility for checking that streams of blocks have a consistent channel
/// count and sample rate
///
/// both can be specified in the constructor if there is an expected channel
/// count or sample rate
///
/// process checks that the channel count and sample rate is the specified
/// value, or matches the first block
class ChannelConformer {
 public:
  ChannelConformer(std::optional<size_t> channel_count_ = std::nullopt,
                   std::optional<unsigned int> sample_rate_ = std::nullopt)
      : channel_count(channel_count_), sample_rate(sample_rate_) {}

  void process(const BlockDescription &info) {
    if (channel_count) {
      if (info.channel_count != *channel_count) {
        std::ostringstream err;
        err << "unexpected channel count: got " << info.channel_count << " but expected " << *channel_count;
        throw std::runtime_error(err.str());
      }
    } else
      channel_count = info.channel_count;

    if (sample_rate) {
      if (info.sample_rate != *sample_rate) {
        std::ostringstream err;
        err << "unexpected sample rate: got " << info.sample_rate << " but expected " << *sample_rate;
        throw std::runtime_error(err.str());
      }
    } else
      sample_rate = info.sample_rate;
  }

 private:
  std::optional<size_t> channel_count;
  std::optional<unsigned int> sample_rate;
};

/// a block with a start sample (or offset) for use in AlignedBlocks
struct BlockWithOffset {
  BlockWithOffset() noexcept = default;
  InterleavedBlockPtr block;  /// will be nullptr if this channel has ended
  size_t start;               /// first sample index in this block to process
};

/// some time range within 1 block from n inputs
///
/// given an object b of this type, for input i and channel c, this represents
/// some samples with index s from 0 to n_samples, which:
///
/// - are sample start_sample + s relative to the begining of the stream
///
/// - are only available if b.at(i).block (because streams ended at different
///   times)
///
/// - are available at b.at(i).block.sample(c, b.at(i).start + s)
struct AlignedBlocks {
  AlignedBlocks() noexcept = default;
  size_t start_sample;                  /// index of first sample in timeline
  size_t n_samples;                     /// number of samples to process in all blocks
  std::vector<BlockWithOffset> blocks;  /// one for each channel, use .at

  BlockWithOffset &at(size_t i) { return blocks.at(i); }
};

/// Given n streams of InterleavedSampleBlocks with arbitrary sizes and
/// interleaving, produce one stream of references to blocks with aligned
/// samples.
///
/// This can be used for example to mix together two streams into one; to do
/// this:
/// - feed blocks and EOF status into push and set_eof
/// - call process with a callback
///     - the callback will be called with time ranges of samples (represented
///       by start_sample and n_samples), and a block that contains that range
///       for each input stream (or only one if the streams do not end at the
///       same time)
class InterleavedBlockAligner {
 public:
  InterleavedBlockAligner(size_t n_inputs) : eof(n_inputs, false), offsets(n_inputs, 0), blocks(n_inputs) {
    aligned_tmp.blocks.resize(n_inputs);
    aligned_tmp.start_sample = 0;
  }

  /// add a block for stream idx
  void push(size_t idx, InterleavedBlockPtr block) { blocks.at(idx).push_back(std::move(block)); }

  /// mark stream idx as being EOF (once remaining blocks have been processed
  /// -- the same logic as stream ports)
  void set_eof(size_t idx) { eof.at(idx) = true; }

  /// access the aligned streams; cb will be called multiple times for available time ranges
  void process(const std::function<void(AlignedBlocks &)> &cb) {
    // while there are blocks in all channels
    //   run to end of first block
    //     start: start
    //     end: last sample in first block
    //   start = end
    //   remove first block in each channel where end <= start

    auto should_run = [&]() {
      // - blocks in all non-eof channels
      // - at least one block

      bool at_least_one_block = false;
      bool more_blocks_required = false;
      for (size_t i = 0; i < blocks.size(); i++) {
        if (!eof.at(i) && blocks.at(i).size() == 0) more_blocks_required = true;
        if (blocks.at(i).size() != 0) at_least_one_block = true;
      }

      return at_least_one_block && !more_blocks_required;
    };

    while (should_run()) {
      // run on the first block of all channels, up to the end of the first block that ends soonest
      size_t end = min_first_block_end();

      // fill in aligned_tmp
      aligned_tmp.start_sample = start;
      aligned_tmp.n_samples = end - start;

      for (size_t i = 0; i < blocks.size(); i++) {
        if (blocks.at(i).size()) {
          aligned_tmp.blocks.at(i).block = blocks.at(i).at(0);
          assert(start >= offsets.at(i));
          aligned_tmp.blocks.at(i).start = start - offsets.at(i);
        } else {
          aligned_tmp.blocks.at(i).block = {};
          aligned_tmp.blocks.at(i).start = 0;
        }
      }

      // advance and drop any blocks which have been fully processed
      // (do this before calling the callback to avoid some copies in ValuePtr)
      start = end;

      for (size_t i = 0; i < blocks.size(); i++) {
        if (blocks.at(i).size()) {
          size_t block_end = offsets.at(i) + blocks.at(i).at(0).read()->info().sample_count;
          if (block_end <= start) {
            blocks.at(i).erase(blocks.at(i).begin());
            offsets.at(i) = block_end;
          }
        }
      }

      cb(aligned_tmp);
    }
  }

  // assert if there was some issue resulting in not all samples being
  // processed
  void check_done() {
    for (bool e : eof)
      if (!e) throw AssertionError("channel not eof");

    for (auto &block : blocks)
      if (block.size() != 0) throw AssertionError("blocks not empty");
  }

 private:
  // the end sample of the first block in all channels with blocks which occurs first
  size_t min_first_block_end() {
    std::optional<size_t> end;

    for (size_t i = 0; i < blocks.size(); i++) {
      if (blocks.at(i).size()) {
        size_t block_end = offsets.at(i) + blocks.at(i).at(0).read()->info().sample_count;
        if (!end || block_end < *end) end = block_end;
      }
    }

    assert(end);  // guaranteed by should_run
    return *end;
  }

  // next sample to process
  size_t start = 0;

  std::vector<bool> eof;
  std::vector<size_t> offsets;
  std::vector<std::vector<InterleavedBlockPtr>> blocks;

  AlignedBlocks aligned_tmp;
};

class CheckSamples : public StreamingAtomicProcess {
 public:
  CheckSamples(const std::string &name, float rtol_, float atol_, std::function<void(const std::string &)> error_cb_)
      : StreamingAtomicProcess(name),
        in_samples_ref(add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples_ref")),
        in_samples_test(add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples_test")),
        aligner(2 /* channels */),
        rtol(rtol_),
        atol(atol_),
        error_cb(std::move(error_cb_)) {}

  void process() override {
    process_input(0, in_samples_ref);
    process_input(1, in_samples_test);

    auto cb = [this](AlignedBlocks &blocks) {
      auto ref_block = blocks.at(0).block.read();
      auto test_block = blocks.at(1).block.read();
      size_t ref_start = blocks.at(0).start;
      size_t test_start = blocks.at(1).start;

      if (ref_block) ref_len += blocks.n_samples;
      if (test_block) test_len += blocks.n_samples;

      if (ref_block && test_block) {
        size_t ref_channels = ref_block->info().channel_count;
        assert(ref_channels == test_block->info().channel_count);  // checked in conformer

        for (size_t sample = 0; sample < blocks.n_samples; sample++) {
          for (size_t channel = 0; channel < ref_channels; channel++) {
            float ref_sample = ref_block->sample(channel, ref_start + sample);
            float test_sample = test_block->sample(channel, test_start + sample);

            float tol = atol + std::abs(ref_sample) * rtol;

            if (std::abs(test_sample - ref_sample) > tol) {
              std::ostringstream msg;
              msg << "difference at sample " << blocks.start_sample + sample << ", channel " << channel
                  << ": reference=" << ref_sample << ", test=" << test_sample;
              error_cb(msg.str());
            }
          }
        }
      }
    };

    aligner.process(std::ref(cb));
  }

  void finalise() override {
    aligner.check_done();

    if (ref_len != test_len) {
      std::ostringstream msg;
      msg << "reference and test lengths differ: reference=" << ref_len << ", test=" << test_len;
      error_cb(msg.str());
    }
  }

 private:
  /// process input for one port with a given index in the aligner
  void process_input(size_t idx, StreamPortPtr<InterleavedBlockPtr> &port) {
    while (port->available()) {
      auto block_ptr = port->pop();
      auto in_block = block_ptr.read();
      auto &in_description = in_block->info();
      conformer.process(in_description);

      aligner.push(idx, std::move(block_ptr));
    }

    if (port->eof()) aligner.set_eof(idx);
  }

  StreamPortPtr<InterleavedBlockPtr> in_samples_ref;
  StreamPortPtr<InterleavedBlockPtr> in_samples_test;
  ChannelConformer conformer;
  InterleavedBlockAligner aligner;

  float rtol, atol;

  std::function<void(const std::string &)> error_cb;

  size_t ref_len = 0;
  size_t test_len = 0;
};

ProcessPtr make_check_samples(const std::string &name, float rtol, float atol,
                              std::function<void(const std::string &)> error_cb) {
  return std::make_shared<CheckSamples>(name, rtol, atol, error_cb);
}

}  // namespace eat::utilities
