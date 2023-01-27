#include "eat/render/render.hpp"

#include <adm/document.hpp>
#include <adm/utilities/time_conversion.hpp>
#include <ear/dsp/dsp.hpp>
#include <ear/ear.hpp>
#include <limits>
#include <string>

#include "eat/framework/exceptions.hpp"
#include "eat/process/adm_bw64.hpp"
#include "eat/process/block.hpp"
#include "eat/render/rendering_items.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace eat::render;

namespace {

class Buffer {
 public:
  Buffer() {}
  Buffer(const Buffer &) = delete;
  Buffer(Buffer &&) = default;
  Buffer(size_t n_channels_, size_t n_samples_) { resize(n_channels_, n_samples_); }

  float *const *ptrs() { return pointers.data(); }

  float *channel_ptr(size_t i) { return pointers.at(i); }

  void zero() {
    for (size_t i = 0; i < samples.size(); i++) samples[i] = 0.0f;
  }

  void add(Buffer &other) {
    for (size_t i = 0; i < samples.size(); i++) samples[i] += other.samples[i];
  }

  void resize(size_t n_channels_, size_t n_samples_) {
    if (n_channels_ != n_channels || n_samples_ != n_samples) {
      n_channels = n_channels_;
      n_samples = n_samples_;

      samples.resize(n_samples * n_channels);
      pointers.resize(n_channels);
      for (size_t channel_i = 0; channel_i < n_channels; channel_i++)
        pointers[channel_i] = samples.data() + channel_i * n_samples;
    }
  }

  void from_interleaved(const InterleavedSampleBlock &b) {
    auto &info = b.info();
    resize(info.channel_count, info.sample_count);

    for (size_t channel_i = 0; channel_i < n_channels; channel_i++)
      for (size_t sample_i = 0; sample_i < n_samples; sample_i++)
        pointers[channel_i][sample_i] = b.sample(channel_i, sample_i);
  }

  InterleavedSampleBlock to_interleaved(unsigned int sample_rate, size_t start = 0) {
    assert(start < n_samples);
    BlockDescription info{n_samples - start, pointers.size(), sample_rate};

    InterleavedSampleBlock block{info};

    for (size_t channel_i = 0; channel_i < n_channels; channel_i++)
      for (size_t sample_i = 0; sample_i < info.sample_count; sample_i++)
        block.sample(channel_i, sample_i) = pointers[channel_i][start + sample_i];

    return block;
  }

 private:
  size_t n_channels = 0;
  size_t n_samples = 0;
  std::vector<float> samples;
  std::vector<float *> pointers;
};

void zero_samples(float *const *samples, size_t n_channels, size_t n_samples) {
  for (size_t channel_i = 0; channel_i < n_channels; channel_i++)
    for (size_t sample_i = 0; sample_i < n_samples; sample_i++) samples[channel_i][sample_i] = 0.0f;
}

/// a += b
void add_samples(float *const *a, float *const *b, size_t n_channels, size_t n_samples) {
  for (size_t channel_i = 0; channel_i < n_channels; channel_i++)
    for (size_t sample_i = 0; sample_i < n_samples; sample_i++) a[channel_i][sample_i] += b[channel_i][sample_i];
}

/// write from in to out, zeroing channels in out where is_lfe is true
void write_non_lfe(float *const *out, const float *const *in, const std::vector<bool> is_lfe, size_t n_channels_in,
                   size_t n_channels_out, size_t n_samples) {
  size_t in_channel = 0;
  for (size_t out_channel = 0; out_channel < n_channels_out; out_channel++) {
    if (is_lfe.at(out_channel)) {
      for (size_t sample = 0; sample < n_samples; sample++) out[out_channel][sample] = 0.0f;
    } else {
      always_assert(in_channel < n_channels_in, "fewer LFE channels than expected");
      for (size_t sample = 0; sample < n_samples; sample++) out[out_channel][sample] = in[in_channel][sample];
      in_channel++;
    }
  }
  always_assert(in_channel == n_channels_in, "more LFE channels than expected");
}

// like the rendering items track specs, but specialised for rendering with channel number s rather than track uid
// references
struct RenderDirectTrackSpec {
  size_t track_idx;
};

using RenderTrackSpec = std::variant<RenderDirectTrackSpec, SilentTrackSpec>;

struct ToRenderTrackSpecVisitor {
  RenderTrackSpec operator()(const SilentTrackSpec &spec) noexcept { return spec; }

  RenderTrackSpec operator()(const DirectTrackSpec &spec) noexcept {
    auto id = spec.track->get<adm::AudioTrackUidId>();

    auto it = channel_map.find(id);
    assert(it != channel_map.end());
    return RenderDirectTrackSpec{it->second};
  }

  const channel_map_t &channel_map;
};

RenderTrackSpec to_render_track_spec(const TrackSpec &spec, const channel_map_t &channel_map) {
  return std::visit(ToRenderTrackSpecVisitor{channel_map}, spec);
}

struct RenderTrackSpecVisitor {
  void operator()(const RenderDirectTrackSpec &spec) noexcept {
    for (size_t sample_i = 0; sample_i < n_samples; sample_i++) out[sample_i] = in[spec.track_idx][sample_i];
  }

  void operator()(const SilentTrackSpec &) noexcept {
    for (size_t sample_i = 0; sample_i < n_samples; sample_i++) out[sample_i] = 0.0f;
  }

  const float *const *in;
  float *out;
  size_t n_samples;
};

void render_track_spec(const float *const *in, float *out, size_t n_samples, const RenderTrackSpec &spec) {
  std::visit(RenderTrackSpecVisitor{in, out, n_samples}, spec);
}

struct NumRequiredChannelsVisitor {
  size_t operator()(const RenderDirectTrackSpec &spec) noexcept { return spec.track_idx + 1; }

  size_t operator()(const SilentTrackSpec &) noexcept { return 0; }
};

// get the number of tracks required to render a track spec
size_t num_tracks_required(const RenderTrackSpec &spec) { return std::visit(NumRequiredChannelsVisitor{}, spec); }

std::shared_ptr<adm::AudioObject> get_object(ADMPath &path) {
  if (path.audioObjects.size())
    return path.audioObjects[path.audioObjects.size() - 1];
  else
    return nullptr;
}

std::shared_ptr<adm::AudioObject> get_object(MonoRenderingItem &ri) { return get_object(ri.adm_path); }

std::shared_ptr<adm::AudioObject> get_object(HOARenderingItem &ri) { return get_object(ri.adm_paths.at(0)); }

template <typename Block>
std::optional<adm::Time> get_rtime(Block &block) {
  if (block.template has<adm::Rtime>() && !block.template isDefault<adm::Rtime>())
    return block.template get<adm::Rtime>().get();
  else
    return std::nullopt;
}

template <typename Block>
std::optional<adm::Time> get_duration(Block &block) {
  if (block.template has<adm::Duration>() && !block.template isDefault<adm::Duration>())
    return block.template get<adm::Duration>().get();
  else
    return std::nullopt;
}

template <>
std::optional<adm::Time> get_rtime(HOATypeMetadata &tm) {
  return tm.rtime;
}

template <>
std::optional<adm::Time> get_duration(HOATypeMetadata &tm) {
  return tm.duration;
}

std::optional<adm::RationalTime> optional_to_rational(const std::optional<adm::Time> &time) {
  if (time)
    return adm::asRational(*time);
  else
    return std::nullopt;
}

struct BlockExtent {
  adm::RationalTime start;
  std::optional<adm::RationalTime> end;  // nullopt if this block has no defined end
};

/// interpolation point for a given block
struct InterpPoint {
  InterpPoint(adm::RationalTime time_, bool zero_) noexcept : time(time_), zero(zero_) {}

  adm::RationalTime time;
  // false: use gains calculated for this block
  // true: use zero gains
  bool zero = false;
};

template <typename RI>
class InterpretTimingMetadata {
 public:
  InterpretTimingMetadata(RI &ri_) : ri(ri_) {
    auto object = get_object(ri);
    if (object) {
      object_start = adm::asRational(object->template get<adm::Start>().get());

      if (object->template has<adm::Duration>())
        object_end = object_start + adm::asRational(object->template get<adm::Duration>().get());
    }
  }

  template <typename Block>
  BlockExtent get_block_extent(Block &block) {
    auto rtime = optional_to_rational(get_rtime(block));
    auto duration = optional_to_rational(get_duration(block));

    BlockExtent extent;

    if (rtime && duration) {
      extent.start = object_start + *rtime;
      extent.end = extent.start + *duration;

      if (object_end && extent.end > *object_end) throw std::runtime_error{"block ends after object end"};
    } else if (!rtime && !duration) {
      extent.start = object_start;
      extent.end = object_end;
    } else
      throw std::runtime_error{"rtime and duration must be used together"};

    if (!first_block && (!last_block_end                   // last was infinite
                         || extent.start < last_block_end  // overlap
                         ))
      throw std::runtime_error{"blocks may not overlap"};

    first_block = false;
    last_block_end = extent.end;

    return extent;
  }

  template <typename Block>
  std::vector<InterpPoint> get_interp_points(Block &block) {
    BlockExtent extent = get_block_extent(block);

    std::vector<InterpPoint> points;
    points.emplace_back(extent.start, true);
    points.emplace_back(extent.start, false);

    if (extent.end) {
      points.emplace_back(*extent.end, false);
      points.emplace_back(*extent.end, true);
    }

    return points;
  }

  std::vector<InterpPoint> get_interp_points(adm::AudioBlockFormatObjects &block) {
    bool was_first_block = first_block;

    BlockExtent extent = get_block_extent(block);

    std::vector<InterpPoint> points;

    if (was_first_block) {
      points.emplace_back(extent.start, true);
      points.emplace_back(extent.start, false);
      if (extent.end) points.emplace_back(*extent.end, false);
    } else {
      adm::RationalTime block_start = extent.start;
      assert(static_cast<bool>(extent.end));  // checked in get_block_extent
      adm::RationalTime block_end = *extent.end;

      adm::RationalTime target_time;
      auto jp = block.get<adm::JumpPosition>();
      if (jp.get<adm::JumpPositionFlag>().get()) {
        auto interp_len = adm::asRational(jp.get<adm::InterpolationLength>().get());
        target_time = block_start + interp_len;
      } else
        target_time = block_end;

      if (target_time > block_end) throw std::runtime_error{"interpolation length cannot be longer than block"};

      if (target_time < block_end) points.emplace_back(target_time, false);
      points.emplace_back(block_end, false);
    }

    return points;
  }

  std::vector<InterpPoint> get_end_points() {
    std::vector<InterpPoint> points;

    if (last_block_end) points.emplace_back(*last_block_end, true);

    return points;
  }

 private:
  adm::RationalTime object_start = {0, 1};
  std::optional<adm::RationalTime> object_end = std::nullopt;

  bool first_block = true;
  std::optional<adm::RationalTime> last_block_end = std::nullopt;

  RI &ri;
};

struct ConvertPositionVisitor : public boost::static_visitor<ear::Position> {
  ear::Position operator()(const adm::SphericalPosition &pos) const {
    return ear::PolarPosition{pos.get<adm::Azimuth>().get(), pos.get<adm::Elevation>().get(),
                              pos.get<adm::Distance>().get()};
  }

  ear::Position operator()(const adm::CartesianPosition &pos) const {
    return ear::CartesianPosition{pos.get<adm::X>().get(), pos.get<adm::Y>().get(), pos.get<adm::Z>().get()};
  }
};

double get_path_gain(const ADMPath &path) {
  double gain = 1.0;

  for (const std::shared_ptr<adm::AudioObject> &object : path.audioObjects) {
    if (object->get<adm::Mute>().get()) gain *= 0.0;
    gain *= object->get<adm::Gain>().asLinear();
  }

  return gain;
}

ear::ObjectDivergence get_divergence(const adm::ObjectDivergence &divergence, bool cartesian) {
  // use the defaults from BS.2127 rather than BS.2076, which changed between -1 and -2
  if (cartesian) {
    if (divergence.has<adm::AzimuthRange>())
      throw std::runtime_error(
          "cartesian Objects audioBlockFormat has an objectDivergence element with an azimuthRange attribute");
    ear::CartesianObjectDivergence ear_divergence{static_cast<double>(divergence.get<adm::Divergence>().get())};
    if (divergence.has<adm::PositionRange>())
      ear_divergence.positionRange = static_cast<double>(divergence.get<adm::PositionRange>().get());
    return ear_divergence;
  } else {
    ear::PolarObjectDivergence ear_divergence{static_cast<double>(divergence.get<adm::Divergence>().get())};
    if (divergence.has<adm::AzimuthRange>())
      ear_divergence.azimuthRange = static_cast<double>(divergence.get<adm::AzimuthRange>().get());
    return ear_divergence;
  }
}

ear::ObjectsTypeMetadata to_otm(const ObjectRenderingItem &ri, const adm::AudioBlockFormatObjects &bf) {
  ear::ObjectsTypeMetadata otm;

  otm.position = boost::apply_visitor(ConvertPositionVisitor(), bf.get<adm::Position>());
  otm.width = bf.get<adm::Width>().get();
  otm.height = bf.get<adm::Height>().get();
  otm.depth = bf.get<adm::Depth>().get();
  otm.cartesian = bf.get<adm::Cartesian>().get();
  otm.gain = get_path_gain(ri.adm_path) * bf.get<adm::Gain>().asLinear();
  otm.diffuse = bf.get<adm::Diffuse>().get();

  auto channelLock = bf.get<adm::ChannelLock>();
  otm.channelLock.flag = channelLock.get<adm::ChannelLockFlag>().get();
  if (channelLock.has<adm::MaxDistance>()) otm.channelLock.maxDistance = channelLock.get<adm::MaxDistance>().get();

  otm.objectDivergence = get_divergence(bf.get<adm::ObjectDivergence>(), otm.cartesian);

  // TODO: libadm does not currently handle zone exclusion

  otm.screenRef = bf.get<adm::ScreenRef>().get();
  // TODO: libadm does not currently handle audioProgrammeReferenceScreen

  return otm;
}

ear::ScreenEdgeLock get_edge_lock(const adm::ScreenEdgeLock &edge_lock) {
  ear::ScreenEdgeLock ear_edge_lock;

  if (edge_lock.has<adm::HorizontalEdge>()) ear_edge_lock.horizontal = edge_lock.get<adm::HorizontalEdge>().get();
  if (edge_lock.has<adm::VerticalEdge>()) ear_edge_lock.vertical = edge_lock.get<adm::VerticalEdge>().get();

  return ear_edge_lock;
}

struct ConvertSpeakerPositionVisitor : public boost::static_visitor<ear::SpeakerPosition> {
  ear::SpeakerPosition operator()(const adm::SphericalSpeakerPosition &pos) const {
    ear::PolarSpeakerPosition ear_pos{pos.get<adm::Azimuth>().get(), pos.get<adm::Elevation>().get(),
                                      pos.get<adm::Distance>().get()};

    if (pos.has<adm::AzimuthMin>()) ear_pos.azimuthMin = pos.get<adm::AzimuthMin>().get();
    if (pos.has<adm::AzimuthMax>()) ear_pos.azimuthMax = pos.get<adm::AzimuthMax>().get();
    if (pos.has<adm::ElevationMin>()) ear_pos.elevationMin = pos.get<adm::ElevationMin>().get();
    if (pos.has<adm::ElevationMax>()) ear_pos.elevationMax = pos.get<adm::ElevationMax>().get();
    if (pos.has<adm::DistanceMin>()) ear_pos.distanceMin = pos.get<adm::DistanceMin>().get();
    if (pos.has<adm::DistanceMax>()) ear_pos.distanceMax = pos.get<adm::DistanceMax>().get();

    if (pos.has<adm::ScreenEdgeLock>()) ear_pos.screenEdgeLock = get_edge_lock(pos.get<adm::ScreenEdgeLock>());

    return ear_pos;
  }

  ear::SpeakerPosition operator()(const adm::CartesianSpeakerPosition &pos) const {
    ear::CartesianSpeakerPosition ear_pos{pos.get<adm::X>().get(), pos.get<adm::Y>().get(), pos.get<adm::Z>().get()};

    if (pos.has<adm::XMin>()) ear_pos.XMin = pos.get<adm::XMin>().get();
    if (pos.has<adm::XMax>()) ear_pos.XMax = pos.get<adm::XMax>().get();
    if (pos.has<adm::YMin>()) ear_pos.YMin = pos.get<adm::YMin>().get();
    if (pos.has<adm::YMax>()) ear_pos.YMax = pos.get<adm::YMax>().get();
    if (pos.has<adm::ZMin>()) ear_pos.ZMin = pos.get<adm::ZMin>().get();
    if (pos.has<adm::ZMax>()) ear_pos.ZMax = pos.get<adm::ZMax>().get();

    if (pos.has<adm::ScreenEdgeLock>()) ear_pos.screenEdgeLock = get_edge_lock(pos.get<adm::ScreenEdgeLock>());

    return ear_pos;
  }
};

ear::DirectSpeakersTypeMetadata to_dstm(const DirectSpeakersRenderingItem &ri,
                                        const adm::AudioBlockFormatDirectSpeakers &bf) {
  ear::DirectSpeakersTypeMetadata tm;
  for (auto &label : bf.get<adm::SpeakerLabels>()) tm.speakerLabels.push_back(label.get());

  // TODO: libadm is missing AudioBlockFormatDirectSpeakers::get<SpeakerPosition>()
  adm::SpeakerPosition pos = bf.has<adm::CartesianSpeakerPosition>()
                                 ? adm::SpeakerPosition{bf.get<adm::CartesianSpeakerPosition>()}
                                 : adm::SpeakerPosition{bf.get<adm::SphericalSpeakerPosition>()};
  tm.position = boost::apply_visitor(ConvertSpeakerPositionVisitor(), pos);

  if (ri.adm_path.audioPackFormats.size()) {
    auto &apf = ri.adm_path.audioPackFormats.at(ri.adm_path.audioPackFormats.size() - 1);
    tm.audioPackFormatID = adm::formatId(apf->get<adm::AudioPackFormatId>());
  }

  if (ri.adm_path.audioChannelFormat->has<adm::Frequency>()) {
    auto freq = ri.adm_path.audioChannelFormat->get<adm::Frequency>();
    if (freq.has<adm::LowPass>()) tm.channelFrequency.lowPass = freq.get<adm::LowPass>().get();
    if (freq.has<adm::HighPass>()) tm.channelFrequency.highPass = freq.get<adm::HighPass>().get();
  }

  return tm;
}

ear::HOATypeMetadata to_hoatm(const HOARenderingItem &, const HOATypeMetadata &tm) {
  ear::HOATypeMetadata ear_tm;

  ear_tm.orders = tm.orders;
  ear_tm.degrees = tm.degrees;
  ear_tm.normalization = tm.normalization;
  if (tm.nfcRefDist) ear_tm.nfcRefDist = *tm.nfcRefDist;

  if (tm.screenRef) throw std::runtime_error("screenRef is not supported");

  return ear_tm;
}

auto round(adm::RationalTime t) {
  auto x = t + adm::RationalTime{1, 2};
  return x.numerator() / x.denominator();
}

}  // namespace

class ObjectRenderer {
 public:
  ObjectRenderer(const ear::Layout &layout, ear::dsp::block_convolver::Context &convolver_ctx, size_t block_size_)
      : block_size(block_size_),
        n_channels(layout.withoutLfe().channels().size()),
        n_channels_out(layout.channels().size()),
        is_lfe(layout.isLfe()),
        decorrelator_delay(n_channels, static_cast<size_t>(ear::decorrelatorCompensationDelay())),
        gain_calc(layout.withoutLfe()),
        temp_mono(1, block_size),
        temp(n_channels, block_size),
        temp_direct(n_channels, block_size),
        temp_diffuse(n_channels, block_size),
        temp_out(n_channels, block_size) {
    auto decorrelation_filters = ear::designDecorrelators(layout);
    for (size_t i = 0; i < decorrelation_filters.size(); i++) {
      if (!is_lfe.at(i)) {
        auto &filter = decorrelation_filters.at(i);
        ear::dsp::block_convolver::Filter filter_obj(convolver_ctx, filter.size(), filter.data());
        decorrelators.emplace_back(
            std::make_unique<ear::dsp::block_convolver::BlockConvolver>(convolver_ctx, filter_obj));
      }
    }
  }

  void setup_rendering_items(unsigned int fs, const std::vector<std::shared_ptr<ObjectRenderingItem>> &rendering_items,
                             const channel_map_t &channel_map) {
    n_objects = rendering_items.size();

    direct_gain_interpolators.clear();
    diffuse_gain_interpolators.clear();
    track_specs.clear();

    for (auto &ri : rendering_items) {
      GainInterpolator direct_gain_interp;
      GainInterpolator diffuse_gain_interp;

      InterpretTimingMetadata<ObjectRenderingItem> interp(*ri);

      std::vector<float> direct_gains(n_channels), diffuse_gains(n_channels);

      auto push_point = [&](const InterpPoint &point) {
        long int sample = round(fs * point.time);
        if (point.zero) {
          direct_gain_interp.interp_points.emplace_back(sample, std::vector<float>(n_channels, 0.0));
          diffuse_gain_interp.interp_points.emplace_back(sample, std::vector<float>(n_channels, 0.0));
        } else {
          direct_gain_interp.interp_points.emplace_back(sample, direct_gains);
          diffuse_gain_interp.interp_points.emplace_back(sample, diffuse_gains);
        }
      };

      for (auto &bf : ri->adm_path.audioChannelFormat->getElements<adm::AudioBlockFormatObjects>()) {
        direct_gains.resize(n_channels);
        diffuse_gains.resize(n_channels);
        gain_calc.calculate(to_otm(*ri, bf), direct_gains, diffuse_gains);
        for (const auto &point : interp.get_interp_points(bf)) push_point(point);
      }

      for (const auto &point : interp.get_end_points()) push_point(point);

      direct_gain_interpolators.push_back(std::move(direct_gain_interp));
      diffuse_gain_interpolators.push_back(std::move(diffuse_gain_interp));
      track_specs.push_back(to_render_track_spec(ri->track_spec, channel_map));
    }
  }

  void setup_input_channels(size_t n_in_channels) {
    for (auto &ts : track_specs)
      if (num_tracks_required(ts) > n_in_channels) throw std::runtime_error("more tracks required than given");
  }

  size_t delay() { return static_cast<size_t>(ear::decorrelatorCompensationDelay()); }

  void process(const float *const *in, float *const *out) {
    // flows:
    // for each object:
    //   in -> render track spec -> temp_mono
    //   temp_mono -> direct gains -> temp -> sum to temp_direct
    //   temp_mono -> difuse gains -> temp -> sum to temp_diffuse
    //
    // temp_direct -> delays -> temp_out
    // temp_diffuse -> decorrelators -> temp -> add to temp_out
    // temp_out -> distribute to out

    temp_direct.zero();
    temp_diffuse.zero();
    for (size_t i = 0; i < n_objects; i++) {
      render_track_spec(in, *temp_mono.ptrs(), block_size, track_specs[i]);

      direct_gain_interpolators[i].process(block_start, block_size, temp_mono.ptrs(), temp.ptrs());
      temp_direct.add(temp);

      diffuse_gain_interpolators[i].process(block_start, block_size, temp_mono.ptrs(), temp.ptrs());
      temp_diffuse.add(temp);
    }

    decorrelator_delay.process(block_size, temp_direct.ptrs(), temp_out.ptrs());
    for (size_t channel_i = 0; channel_i < n_channels; channel_i++)
      decorrelators[channel_i]->process(temp_diffuse.ptrs()[channel_i], temp.ptrs()[channel_i]);
    temp_out.add(temp);

    write_non_lfe(out, temp_out.ptrs(), is_lfe, n_channels, n_channels_out, block_size);

    block_start += static_cast<long int>(block_size);
  }

 private:
  long int block_start = 0;
  size_t block_size;
  size_t n_channels;  // number of non-LFE channels to be processes internally (size of gains, delays, decorrelators)
  size_t n_channels_out;  // number of channels including LFE
  size_t n_objects;

  std::vector<bool> is_lfe;

  std::vector<RenderTrackSpec> track_specs;

  using InterpType = ear::dsp::LinearInterpVector;
  using GainInterpolator = ear::dsp::GainInterpolator<InterpType>;

  std::vector<GainInterpolator> direct_gain_interpolators;
  std::vector<GainInterpolator> diffuse_gain_interpolators;

  std::vector<std::unique_ptr<ear::dsp::block_convolver::BlockConvolver>> decorrelators;
  ear::dsp::DelayBuffer decorrelator_delay;

  ear::GainCalculatorObjects gain_calc;

  Buffer temp_mono;
  Buffer temp;
  Buffer temp_direct;
  Buffer temp_diffuse;
  Buffer temp_out;
};

class DirectSpeakersRenderer {
 public:
  DirectSpeakersRenderer(const ear::Layout &layout, size_t block_size_)
      : block_size(block_size_),
        n_channels(layout.channels().size()),
        gain_calc(layout),
        temp_mono(1, block_size),
        temp(n_channels, block_size) {}

  void setup_rendering_items(unsigned int fs,
                             const std::vector<std::shared_ptr<DirectSpeakersRenderingItem>> &rendering_items,
                             const channel_map_t &channel_map) {
    n_objects = rendering_items.size();

    gain_interpolators.clear();
    track_specs.clear();

    for (auto &ri : rendering_items) {
      GainInterpolator gain_interp;

      InterpretTimingMetadata<DirectSpeakersRenderingItem> interp(*ri);

      std::vector<float> gains(n_channels);

      auto push_point = [&](const InterpPoint &point) {
        long int sample = round(fs * point.time);
        if (point.zero) {
          gain_interp.interp_points.emplace_back(sample, std::vector<float>(n_channels, 0.0));
        } else {
          gain_interp.interp_points.emplace_back(sample, gains);
        }
      };

      for (auto &bf : ri->adm_path.audioChannelFormat->getElements<adm::AudioBlockFormatDirectSpeakers>()) {
        gains.resize(n_channels);
        gain_calc.calculate(to_dstm(*ri, bf), gains);

        for (const auto &point : interp.get_interp_points(bf)) push_point(point);
      }

      for (const auto &point : interp.get_end_points()) push_point(point);

      gain_interpolators.push_back(std::move(gain_interp));
      track_specs.push_back(to_render_track_spec(ri->track_spec, channel_map));
    }
  }

  void setup_input_channels(size_t n_in_channels) {
    for (auto &ts : track_specs)
      if (num_tracks_required(ts) > n_in_channels) throw std::runtime_error("more tracks required than given");
  }

  size_t delay() { return 0; }

  void process(const float *const *in, float *const *out) {
    zero_samples(out, n_channels, block_size);
    for (size_t i = 0; i < n_objects; i++) {
      render_track_spec(in, *temp_mono.ptrs(), block_size, track_specs[i]);
      gain_interpolators[i].process(block_start, block_size, temp_mono.ptrs(), temp.ptrs());
      add_samples(out, temp.ptrs(), n_channels, block_size);
    }

    block_start += static_cast<long int>(block_size);
  }

 private:
  long int block_start = 0;
  size_t block_size;
  size_t n_channels;
  size_t n_objects;

  std::vector<RenderTrackSpec> track_specs;

  using InterpType = ear::dsp::LinearInterpVector;
  using GainInterpolator = ear::dsp::GainInterpolator<InterpType>;

  std::vector<GainInterpolator> gain_interpolators;

  ear::GainCalculatorDirectSpeakers gain_calc;

  Buffer temp_mono;
  Buffer temp;
};

class HOARenderer {
 public:
  HOARenderer(const ear::Layout &layout, size_t block_size_)
      : block_size(block_size_),
        n_channels(layout.channels().size()),
        gain_calc(layout),
        temp_out(n_channels, block_size) {}

  void setup_rendering_items(unsigned int fs, const std::vector<std::shared_ptr<HOARenderingItem>> &rendering_items,
                             const channel_map_t &channel_map) {
    n_objects = rendering_items.size();

    gain_interpolators.clear();
    track_specs.clear();

    size_t max_in_channels = 0;

    for (auto &ri : rendering_items) {
      if (ri->tracks.size() > max_in_channels) max_in_channels = ri->tracks.size();

      GainInterpolator gain_interp;

      InterpretTimingMetadata<HOARenderingItem> interp(*ri);

      std::vector<std::vector<float>> gains;
      RenderTrackSpecs track_specs_for_ri;
      for (std::size_t in_channel = 0; in_channel < ri->tracks.size(); in_channel++) {
        gains.emplace_back(n_channels, 0.0);
        track_specs_for_ri.push_back(to_render_track_spec(ri->tracks.at(in_channel), channel_map));
      }

      std::vector<std::vector<float>> zero_gains = gains;

      auto push_point = [&](const InterpPoint &point) {
        long int sample = round(fs * point.time);
        if (point.zero) {
          gain_interp.interp_points.emplace_back(sample, zero_gains);
        } else {
          gain_interp.interp_points.emplace_back(sample, gains);
        }
      };

      for (auto &type_metadata : ri->type_metadata) {
        gain_calc.calculate(to_hoatm(*ri, type_metadata), gains);

        for (const auto &point : interp.get_interp_points(type_metadata)) push_point(point);
      }

      for (const auto &point : interp.get_end_points()) push_point(point);

      gain_interpolators.push_back(std::move(gain_interp));
      track_specs.push_back(std::move(track_specs_for_ri));
    }

    temp_in.resize(max_in_channels, block_size);
  }

  void setup_input_channels(size_t n_in_channels) {
    for (auto &ts_for_obj : track_specs)
      for (auto &ts : ts_for_obj)
        if (num_tracks_required(ts) > n_in_channels) throw std::runtime_error("more tracks required than given");
  }

  size_t delay() { return 0; }

  void process(const float *const *in, float *const *out) {
    zero_samples(out, n_channels, block_size);

    for (size_t i = 0; i < n_objects; i++) {
      // flows:
      // in -> render track specs -> temp_in
      // temp1 -> interpolator -> temp_out;
      // temp_out -> add to out

      auto &track_specs_for_obj = track_specs.at(i);
      auto &gain_interp = gain_interpolators.at(i);

      for (size_t in_channel = 0; in_channel < track_specs_for_obj.size(); in_channel++)
        render_track_spec(in, temp_in.channel_ptr(in_channel), block_size, track_specs_for_obj.at(in_channel));

      gain_interp.process(block_start, block_size, temp_in.ptrs(), temp_out.ptrs());
      add_samples(out, temp_out.ptrs(), n_channels, block_size);
    }

    block_start += static_cast<long int>(block_size);
  }

 private:
  long int block_start = 0;
  size_t block_size;
  size_t n_channels;
  size_t n_objects;

  // one vector of track specs per input, containing one track spec per channel
  using RenderTrackSpecs = std::vector<RenderTrackSpec>;
  std::vector<RenderTrackSpecs> track_specs;

  using InterpType = ear::dsp::LinearInterpMatrix;
  using GainInterpolator = ear::dsp::GainInterpolator<InterpType>;

  std::vector<GainInterpolator> gain_interpolators;

  ear::GainCalculatorHOA gain_calc;

  Buffer temp_in;
  Buffer temp_out;
};

class CombinedRenderer {
 public:
  CombinedRenderer(const ear::Layout &layout, ear::dsp::block_convolver::Context &convolver_ctx, size_t block_size_)
      : n_channels(layout.channels().size()),
        block_size(block_size_),
        objects_renderer(layout, convolver_ctx, block_size),
        direct_speakers_renderer(layout, block_size),
        hoa_renderer(layout, block_size),
        objects_comp_delay(n_channels, objects_renderer.delay()),
        temp1(n_channels, block_size),
        temp2(n_channels, block_size) {}

  void setup_input_channels(size_t n_in_channels_) {
    objects_renderer.setup_input_channels(n_in_channels_);
    direct_speakers_renderer.setup_input_channels(n_in_channels_);
    hoa_renderer.setup_input_channels(n_in_channels_);
  }

  void setup_rendering_items(unsigned int fs, const std::vector<std::shared_ptr<RenderingItem>> &rendering_items,
                             const channel_map_t &channel_map) {
    std::vector<std::shared_ptr<ObjectRenderingItem>> objects_items;
    std::vector<std::shared_ptr<DirectSpeakersRenderingItem>> direct_speakers_items;
    std::vector<std::shared_ptr<HOARenderingItem>> hoa_items;

    for (auto &item : rendering_items) {
      if (auto object_item = std::dynamic_pointer_cast<ObjectRenderingItem>(item); object_item)
        objects_items.push_back(std::move(object_item));
      else if (auto direct_speakers_item = std::dynamic_pointer_cast<DirectSpeakersRenderingItem>(item);
               direct_speakers_item)
        direct_speakers_items.push_back(std::move(direct_speakers_item));
      else if (auto hoa_item = std::dynamic_pointer_cast<HOARenderingItem>(item); hoa_item)
        hoa_items.push_back(std::move(hoa_item));
      else
        throw std::runtime_error("unsupported rendering item type");
    }

    objects_renderer.setup_rendering_items(fs, objects_items, channel_map);
    direct_speakers_renderer.setup_rendering_items(fs, direct_speakers_items, channel_map);
    hoa_renderer.setup_rendering_items(fs, hoa_items, channel_map);
  }

  size_t delay() { return objects_renderer.delay(); }

  void process(const float *const *in, float *const *out) {
    // flows:
    // in -> object renderer -> temp1 -> add to out
    // in -> direct speakers renderer -> temp1
    // in -> hoa renderer -> temp2 -> add to temp1
    // temp1 -> delay -> temp2 -> add to out

    zero_samples(out, n_channels, block_size);

    objects_renderer.process(in, temp1.ptrs());
    add_samples(out, temp1.ptrs(), n_channels, block_size);

    direct_speakers_renderer.process(in, temp1.ptrs());

    hoa_renderer.process(in, temp2.ptrs());
    temp1.add(temp2);

    objects_comp_delay.process(block_size, temp1.ptrs(), temp2.ptrs());
    add_samples(out, temp2.ptrs(), n_channels, block_size);
  }

 private:
  size_t n_channels;
  size_t block_size;
  size_t n_in_channels = std::numeric_limits<size_t>::max();
  ObjectRenderer objects_renderer;
  DirectSpeakersRenderer direct_speakers_renderer;
  HOARenderer hoa_renderer;
  ear::dsp::DelayBuffer objects_comp_delay;  // to align non-objects paths
  Buffer temp1;
  Buffer temp2;
};

class RendererProcess : public StreamingAtomicProcess {
 public:
  RendererProcess(const std::string &name, const ear::Layout &layout, size_t block_size_,
                  const SelectionOptionsId &options = {})
      : StreamingAtomicProcess(name),
        selection_options(options),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        in_samples(add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples")),
        out_samples(add_out_port<StreamPort<InterleavedBlockPtr>>("out_samples")),
        block_size(block_size_),
        n_channels(layout.channels().size()),
        convolver_ctx(block_size, ear::get_fft_kiss<float>()),
        renderer(layout, convolver_ctx, block_size) {}

  void initialise() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    auto selection_options_ref = selection_options_from_ids(doc, selection_options);

    SelectionResult result = select_items(doc, selection_options_ref);

    renderer.setup_rendering_items(sample_rate, result.items, adm.channel_map);

    n_samples_processed = 0;
    has_input = false;
  }

  void process() override {
    while (in_samples->available()) {
      auto in_block = in_samples->pop().read();
      auto &info = in_block->info();

      if (info.sample_rate != sample_rate)
        throw std::runtime_error("sample rate must be " + std::to_string(sample_rate));

      if (!has_input) {
        n_input_channels = info.channel_count;
        renderer.setup_input_channels(n_input_channels);
        vbs_adapter = std::make_unique<ear::dsp::VariableBlockSizeAdapter>(
            block_size, n_input_channels, n_channels,
            [this](const float *const *in, float *const *out) { renderer.process(in, out); });

        delay_samples = renderer.delay() + static_cast<size_t>(vbs_adapter->get_delay());
        has_input = true;
      } else {
        always_assert(n_input_channels == info.channel_count, "number of samples changed while rendering");
      }

      inputs.from_interleaved(*in_block);
      outputs.resize(n_channels, info.sample_count);

      vbs_adapter->process(info.sample_count, inputs.ptrs(), outputs.ptrs());

      // only push output if the block extends past the negative delay period
      if (n_samples_processed + info.sample_count > delay_samples) {
        size_t start = n_samples_processed > delay_samples ? 0 : delay_samples - n_samples_processed;
        auto out_block = std::make_shared<InterleavedSampleBlock>(outputs.to_interleaved(sample_rate, start));
        out_samples->push(std::move(out_block));
      }

      n_samples_processed += info.sample_count;
    }
    if (in_samples->eof() && !out_samples->eof_triggered()) {
      // feed through silence to make up for the negative delay
      if (has_input && n_samples_processed) {
        inputs.resize(n_input_channels, delay_samples);
        inputs.zero();
        outputs.resize(n_channels, delay_samples);

        vbs_adapter->process(delay_samples, inputs.ptrs(), outputs.ptrs());
        size_t start = n_samples_processed > delay_samples ? 0 : delay_samples - n_samples_processed;
        auto out_block = std::make_shared<InterleavedSampleBlock>(outputs.to_interleaved(sample_rate, start));
        out_samples->push(std::move(out_block));
      }

      out_samples->close();
    }
  }
  void finalise() override {}

 private:
  SelectionOptionsId selection_options;

  bool has_input = false;  // have we received any input blocks? the below
                           // variables are only initialised on the first block
  size_t n_input_channels;
  size_t delay_samples;
  unsigned int sample_rate = 48000;

  size_t n_samples_processed = 0;

  DataPortPtr<ADMData> in_axml;
  StreamPortPtr<InterleavedBlockPtr> in_samples;
  StreamPortPtr<InterleavedBlockPtr> out_samples;

  size_t block_size;
  size_t n_channels;

  ear::dsp::block_convolver::Context convolver_ctx;
  CombinedRenderer renderer;

  std::unique_ptr<ear::dsp::VariableBlockSizeAdapter> vbs_adapter;

  Buffer inputs;
  Buffer outputs;
};

namespace eat::render {
framework::ProcessPtr make_render(const std::string &name, const ear::Layout &layout, size_t block_size,
                                  const SelectionOptionsId &options) {
  return std::make_shared<RendererProcess>(name, layout, block_size, options);
}
}  // namespace eat::render
