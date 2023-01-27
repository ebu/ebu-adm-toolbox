#include "eat/process/misc.hpp"

#include <adm/document.hpp>
#include <adm/utilities/block_duration_assignment.hpp>

#include "eat/framework/exceptions.hpp"
#include "eat/process/adm_bw64.hpp"

using namespace eat::framework;

namespace eat::process {

class FixDSFrequency : public FunctionalAtomicProcess {
 public:
  FixDSFrequency(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    for (auto &channel : doc->getElements<adm::AudioChannelFormat>()) {
      std::string name = *channel->get<adm::AudioChannelFormatName>();
      if (name.find("LFE") != std::string::npos) {
        channel->set(adm::Frequency{adm::LowPass{120.0f}});
      }
    }

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
};

ProcessPtr make_fix_ds_frequency(const std::string &name) { return std::make_shared<FixDSFrequency>(name); }

class FixBlockFormatDurations : public FunctionalAtomicProcess {
 public:
  FixBlockFormatDurations(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    // TODO: pass in the file duration
    adm::updateBlockFormatDurations(doc);

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
};

ProcessPtr make_fix_block_durations(const std::string &name) { return std::make_shared<FixBlockFormatDurations>(name); }

class FixStreamPackRefs : public FunctionalAtomicProcess {
 public:
  FixStreamPackRefs(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    for (auto &sf : doc->getElements<adm::AudioStreamFormat>()) {
      bool is_pcm = sf->get<adm::FormatDescriptor>() == adm::FormatDefinition::PCM;
      bool has_channel_ref = sf->getReference<adm::AudioChannelFormat>() != nullptr;
      if (is_pcm && has_channel_ref) sf->removeReference<adm::AudioPackFormat>();
    }

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
};

ProcessPtr make_fix_stream_pack_refs(const std::string &name) { return std::make_shared<FixStreamPackRefs>(name); }

class ConvertTrackStreamToChannel : public FunctionalAtomicProcess {
 public:
  ConvertTrackStreamToChannel(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    for (auto &track : doc->getElements<adm::AudioTrackUid>()) {
      if (!track->getReference<adm::AudioChannelFormat>()) {
        auto trackFormat = track->getReference<adm::AudioTrackFormat>();
        framework::always_assert(static_cast<bool>(trackFormat), "found audioTrackUid without audioTrackFormatRef");

        auto streamFormat = trackFormat->getReference<adm::AudioStreamFormat>();
        framework::always_assert(static_cast<bool>(streamFormat),
                                 "found audioTrackFormat without audioStreamFormatRef");

        auto channelFormat = streamFormat->getReference<adm::AudioChannelFormat>();
        framework::always_assert(static_cast<bool>(streamFormat),
                                 "found audioStreamFormat without audioChannelFormatRef");

        track->removeReference<adm::AudioTrackFormat>();
        track->setReference(channelFormat);
      }
    }

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
};

ProcessPtr make_convert_track_stream_to_channel(const std::string &name) {
  return std::make_shared<ConvertTrackStreamToChannel>(name);
}

class AddBlockRtimes : public FunctionalAtomicProcess {
 public:
  AddBlockRtimes(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    run<adm::AudioBlockFormatObjects>(doc);
    run<adm::AudioBlockFormatDirectSpeakers>(doc);
    run<adm::AudioBlockFormatHoa>(doc);
    run<adm::AudioBlockFormatBinaural>(doc);
    run<adm::AudioBlockFormatMatrix>(doc);

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  template <typename BlockType>
  void run(const std::shared_ptr<adm::Document> &doc) {
    for (auto &channel : doc->getElements<adm::AudioChannelFormat>()) {
      for (auto &block : channel->getElements<BlockType>()) {
        bool has_rtime = block.template has<adm::Rtime>() && !block.template isDefault<adm::Rtime>();
        bool has_duration = block.template has<adm::Duration>() && !block.template isDefault<adm::Duration>();

        if (!has_rtime && has_duration) {
          // add a zero rtime with the same type as the duration (and denominator for fractional times)
          // TODO: issue warning?
          adm::Time duration = block.template get<adm::Duration>().get();
          if (duration.isFractional())
            block.set(adm::Rtime{adm::FractionalTime{0, duration.asFractional().denominator()}});
          else
            block.set(adm::Rtime{std::chrono::nanoseconds{0}});
        }
      }
    }
  }

  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
};

ProcessPtr make_add_block_rtimes(const std::string &name) { return std::make_shared<AddBlockRtimes>(name); }

class SetVersion : public FunctionalAtomicProcess {
 public:
  SetVersion(const std::string &name, const std::string &version_)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")),
        version(version_) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    doc->set(adm::Version{version});

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
  std::string version;
};

ProcessPtr make_set_version(const std::string &name, const std::string &version) {
  return std::make_shared<SetVersion>(name, version);
}

}  // namespace eat::process
