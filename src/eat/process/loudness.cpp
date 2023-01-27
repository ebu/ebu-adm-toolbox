#include "eat/process/loudness.hpp"

#include <ebur128.h>

#include <cmath>
#include <ear/bs2051.hpp>

#include "adm/document.hpp"
#include "adm/elements/audio_programme_id.hpp"
#include "adm/elements/loudness_metadata.hpp"
#include "eat/framework/dynamic_subgraph.hpp"
#include "eat/process/adm_bw64.hpp"
#include "eat/process/block.hpp"
#include "eat/render/render.hpp"

using namespace eat::framework;

namespace eat::process {

struct ebur128_state_deleter {
  void operator()(ebur128_state *st) { ebur128_destroy(&st); }
};

inline channel get_channel_type(const ear::Channel &c) {
  static std::map<std::string, channel> channel_map = {
      {"M+030", EBUR128_Mp030}, {"M-030", EBUR128_Mm030}, {"M+000", EBUR128_Mp000}, {"M+110", EBUR128_Mp110},
      {"M-110", EBUR128_Mm110}, {"M+SC", EBUR128_MpSC},   {"M-SC", EBUR128_MmSC},   {"M+060", EBUR128_Mp060},
      {"M-060", EBUR128_Mm060}, {"M+090", EBUR128_Mp090}, {"M-090", EBUR128_Mm090}, {"M+135", EBUR128_Mp135},
      {"M-135", EBUR128_Mm135}, {"M+180", EBUR128_Mp180}, {"U+000", EBUR128_Up000}, {"U+030", EBUR128_Up030},
      {"U-030", EBUR128_Um030}, {"U+045", EBUR128_Up045}, {"U-030", EBUR128_Um045}, {"U+090", EBUR128_Up090},
      {"U-090", EBUR128_Um090}, {"U+110", EBUR128_Up110}, {"U-110", EBUR128_Um110}, {"U+135", EBUR128_Up135},
      {"U-135", EBUR128_Um135}, {"U+180", EBUR128_Up180}, {"T+000", EBUR128_Tp000}, {"B+000", EBUR128_Bp000},
      {"B+045", EBUR128_Bp045}, {"B-045", EBUR128_Bm045},
  };
  if (c.isLfe())
    return EBUR128_UNUSED;
  else {
    if (auto it = channel_map.find(c.name()); it != channel_map.end())
      return it->second;
    else
      throw std::runtime_error("could not find libebur128 channel for " + c.name());
  }
}

/// construct a NamedType with a cast to avoid narrowing conversion warnings
/// while keeping precision if libadm decides to switch to doubles
template <typename NamedType, typename Value>
NamedType named_cast(Value v) {
  return NamedType{static_cast<typename NamedType::value_type>(v)};
}

class MeasureLoudness : public StreamingAtomicProcess {
 public:
  MeasureLoudness(const std::string &name, const ear::Layout &layout)
      : StreamingAtomicProcess(name),
        in_samples(add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples")),
        out_loudness(add_out_port<DataPort<adm::LoudnessMetadata>>("out_loudness")),
        fs(48000),
        n_channels(layout.channels().size()),
        state(ebur128_init(n_channels, fs, EBUR128_MODE_I | EBUR128_MODE_LRA | EBUR128_MODE_TRUE_PEAK)) {
    auto channels = layout.channels();
    for (size_t i = 0; i < channels.size(); i++) {
      auto &libear_channel = channels.at(i);
      ebur128_set_channel(state.get(), i, get_channel_type(libear_channel));
    }
  }

  void process() override {
    while (in_samples->available()) {
      auto in_block = in_samples->pop().read();
      auto &info = in_block->info();

      if (info.sample_rate != fs) throw std::runtime_error("sample rate must be " + std::to_string(fs));
      if (info.channel_count != n_channels)
        throw std::runtime_error("number of input channels must be " + std::to_string(n_channels));

      ebur128_add_frames_float(state.get(), in_block->data(), info.sample_count);
    }
  }

  void finalise() override {
    int res;

    double integrated;
    res = ebur128_loudness_global(state.get(), &integrated);
    always_assert(res == EBUR128_SUCCESS, "ebur128_loudness_global failed");

    double range;
    res = ebur128_loudness_range(state.get(), &range);
    always_assert(res == EBUR128_SUCCESS, "ebur128_loudness_range failed");

    std::vector<double> true_peaks(n_channels);
    for (size_t i = 0; i < n_channels; i++) {
      res = ebur128_true_peak(state.get(), i, true_peaks.data() + i);
      always_assert(res == EBUR128_SUCCESS, "ebur128_true_peak failed");
    }
    double true_peak = 20.0 * std::log10(*std::max_element(true_peaks.begin(), true_peaks.end()));

    adm::LoudnessMetadata loudness;
    loudness.set(named_cast<adm::IntegratedLoudness>(integrated));
    loudness.set(named_cast<adm::LoudnessRange>(range));
    loudness.set(named_cast<adm::MaxTruePeak>(true_peak));

    loudness.set(adm::LoudnessMethod{"ITU-R BS.1770"});
    loudness.set(adm::LoudnessRecType{"EBU R128"});

    out_loudness->set_value(std::move(loudness));
  }

 private:
  StreamPortPtr<InterleavedBlockPtr> in_samples;
  DataPortPtr<adm::LoudnessMetadata> out_loudness;
  unsigned int fs;
  unsigned int n_channels;
  std::unique_ptr<ebur128_state, ebur128_state_deleter> state;
};

framework::ProcessPtr make_measure_loudness(const std::string &name, const ear::Layout &layout) {
  return std::make_shared<MeasureLoudness>(name, layout);
}

class SetProgrammeLoudness : public FunctionalAtomicProcess {
 public:
  SetProgrammeLoudness(const std::string &name, const adm::AudioProgrammeId &programme_id_)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        in_loudness(add_in_port<DataPort<adm::LoudnessMetadata>>("in_loudness")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")),
        programme_id(programme_id_) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    auto loudness = std::move(in_loudness->get_value());

    auto programme = doc->lookup(programme_id);
    if (!programme) throw std::runtime_error("could not find programme " + adm::formatId(programme_id));

    programme->unset<adm::LoudnessMetadatas>();
    programme->add(loudness);

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<adm::LoudnessMetadata> in_loudness;
  DataPortPtr<ADMData> out_axml;
  adm::AudioProgrammeId programme_id;
};

framework::ProcessPtr make_set_programme_loudness(const std::string &name, const adm::AudioProgrammeId &programme_id) {
  return std::make_shared<SetProgrammeLoudness>(name, programme_id);
}

class UpdateAllProgrammeLoudnesses : public DynamicSubgraph {
 public:
  UpdateAllProgrammeLoudnesses(const std::string &name)
      : DynamicSubgraph(name),
        in_samples(add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples")),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

 protected:
  virtual GraphPtr build_subgraph() override {
    auto graph = std::make_shared<Graph>();

    auto parent_in_axml = graph->add_process<ParentDataInput<ADMData>>(std::string{"in_axml"});
    auto parent_out_axml = graph->add_process<ParentDataOutput<ADMData>>(std::string{"out_axml"});
    auto parent_in_samples = graph->add_process<ParentStreamInput<InterleavedBlockPtr>>(std::string{"in_samples"});

    // axml is passed through one SetProgrammeLoudness per programme before the
    // output; this will be updated on each loop to point to the output port of
    // the last SetProgrammeLoudness
    PortPtr current_axml_port = parent_in_axml->get_out_port("out");

    auto layout = ear::getLayout("4+5+0");

    auto adm = in_axml->get_value().document.read();
    for (const auto &programme : adm->getElements<adm::AudioProgramme>()) {
      auto id = programme->get<adm::AudioProgrammeId>();
      std::string id_str = adm::formatId(id);

      render::SelectionOptionsId options = {render::ProgrammeIdStart{id}};
      auto render = render::make_render("measure_" + id_str, layout, 1024, options);
      graph->register_process(render);
      auto measure = graph->add_process<MeasureLoudness>("measure_" + id_str, layout);
      auto update = graph->add_process<SetProgrammeLoudness>("update_" + id_str, id);

      graph->connect(parent_in_samples->port, render->get_in_port("in_samples"));
      graph->connect(parent_in_axml->port, render->get_in_port("in_axml"));
      graph->connect(render->get_out_port("out_samples"), measure->get_in_port("in_samples"));
      graph->connect(measure->get_out_port("out_loudness"), update->get_in_port("in_loudness"));
      graph->connect(current_axml_port, update->get_in_port("in_axml"));
      current_axml_port = update->get_out_port("out_axml");
    }

    graph->connect(current_axml_port, parent_out_axml->port);

    return graph;
  }

 private:
  StreamPortPtr<InterleavedBlockPtr> in_samples;
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
};

framework::ProcessPtr make_update_all_programme_loudnesses(const std::string &name) {
  return std::make_shared<UpdateAllProgrammeLoudnesses>(name);
}

}  // namespace eat::process
