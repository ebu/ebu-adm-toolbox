#include "eat/process/channel_mapping.hpp"

#include "eat/process/adm_bw64.hpp"
#include "eat/process/block.hpp"

using namespace eat::framework;

namespace eat::process {

/// Apply a ChannelMapping to some samples
class ApplyChannelMapping : public StreamingAtomicProcess {
 public:
  ApplyChannelMapping(const std::string &name)
      : StreamingAtomicProcess(name),
        in_samples(add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples")),
        out_samples(add_out_port<StreamPort<InterleavedBlockPtr>>("out_samples")),
        in_channel_mapping(add_in_port<DataPort<ChannelMapping>>("in_channel_mapping")) {}

  void initialise() override { channel_mapping = std::move(in_channel_mapping->get_value()); }
  void process() override {
    while (in_samples->available()) {
      auto in_block = in_samples->pop().read();
      auto &in_description = in_block->info();

      auto out_description = in_description;
      out_description.channel_count = channel_mapping.size();

      auto out_block = std::make_shared<InterleavedSampleBlock>(out_description);

      for (size_t sample_i = 0; sample_i < out_description.sample_count; sample_i++)
        for (size_t channel_i = 0; channel_i < out_description.channel_count; channel_i++) {
          out_block->sample(channel_i, sample_i) = in_block->sample(channel_mapping[channel_i], sample_i);
        }

      out_samples->push(std::move(out_block));
    }
    if (in_samples->eof()) out_samples->close();
  }
  void finalise() override {}

 private:
  ChannelMapping channel_mapping;
  StreamPortPtr<InterleavedBlockPtr> in_samples;
  StreamPortPtr<InterleavedBlockPtr> out_samples;
  DataPortPtr<ChannelMapping> in_channel_mapping;
};

ProcessPtr make_apply_channel_mapping(const std::string &name) { return std::make_shared<ApplyChannelMapping>(name); }

}  // namespace eat::process
