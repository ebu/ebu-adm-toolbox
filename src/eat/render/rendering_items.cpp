#include "eat/render/rendering_items.hpp"

#include <adm/document.hpp>
#include <functional>
#include <map>
#include <set>
#include <vector>

#include "eat/framework/exceptions.hpp"
#include "pack_allocation.hpp"
#include "rendering_items_common.hpp"
#include "rendering_items_internals.hpp"

namespace eat::render {

using namespace adm;

struct ItemSelectionState {
  DocumentPtr adm;

  ProgrammePtr audioProgramme;
  ContentPtr audioContent;
  std::vector<ObjectPtr> audioObjects;

  PackFmtPointer audioPackFormat;
  std::map<ChannelFmtPointer, TrackSpec> channel_allocation;

  std::vector<PackFmtPointer> audioPackFormat_path;
  ChannelFmtPointer audioChannelFormat;
  TrackSpec track_spec = {SilentTrackSpec{}};

  ObjectPtr audioObject() const {
    if (audioObjects.size())
      return audioObjects.at(audioObjects.size() - 1);
    else
      return {};
  }

  std::function<void(const std::string &)> warn;
};

static bool compare_programme_by_id(const ProgrammePtr &a, const ProgrammePtr &b) {
  return a->get<AudioProgrammeId>().get<AudioProgrammeIdValue>() <
         b->get<AudioProgrammeId>().get<AudioProgrammeIdValue>();
}

static ItemSelectionState select_programme(ItemSelectionState state, const ProgrammePtr &audioProgramme) {
  if (!audioProgramme) {
    auto programmes = state.adm->getElements<AudioProgramme>();

    if (programmes.size() > 1) {
      state.warn("more than one audioProgramme; selecting the one with the lowest id");

      state.audioProgramme = *std::min_element(programmes.begin(), programmes.end(), compare_programme_by_id);
    } else if (programmes.size() == 1)
      state.audioProgramme = programmes[0];
    else
      state.audioProgramme = {};
  } else
    // TODO: check parent
    state.audioProgramme = audioProgramme;

  return state;
}

using NextCB = std::function<void(ItemSelectionState)>;
using RenderingItemCB = std::function<void(std::shared_ptr<RenderingItem>)>;

static void select_content(ItemSelectionState state, const NextCB &next_cb) {
  if (state.audioProgramme) {
    for (const auto &content : state.audioProgramme->getReferences<AudioContent>()) {
      state.audioContent = content;
      next_cb(state);
    }
  } else
    next_cb(state);
}

static std::vector<ObjectPtr> get_root_objects(const DocumentPtr &adm) {
  std::set<ObjectPtr> non_root_objects;

  for (const ObjectPtr &object : adm->getElements<AudioObject>())
    for (const ObjectPtr &referenced_object : object->getReferences<AudioObject>())
      non_root_objects.insert(referenced_object);

  std::vector<ObjectPtr> out;
  for (const ObjectPtr &object : adm->getElements<AudioObject>())
    if (non_root_objects.find(object) == non_root_objects.end()) out.push_back(object);

  return out;
}

static std::vector<ObjectPtr> select_root_objects(const ItemSelectionState &state) {
  if (state.audioContent) {
    auto objects = state.audioContent->getReferences<AudioObject>();
    return {objects.begin(), objects.end()};
  } else
    return get_root_objects(state.adm);
}

template <typename T, typename F>
void paths_from_helper(const T &obj, std::vector<T> &path, std::vector<std::vector<T>> &paths,
                       const F &get_sub_objects) {
  framework::always_assert(std::find(path.begin(), path.end(), obj) == path.end(),
                           "found loop, which should be prevented by libadm");

  path.push_back(obj);

  for (const auto &sub_object : get_sub_objects(obj)) paths_from_helper(sub_object, path, paths, get_sub_objects);
  paths.push_back(path);

  path.pop_back();
}

template <typename T, typename F>
std::vector<std::vector<T>> paths_from(const T &obj, const F &get_sub_objects) {
  std::vector<T> path;
  std::vector<std::vector<T>> paths;

  paths_from_helper(obj, path, paths, get_sub_objects);
  assert(path.size() == 0);

  return paths;
}

static std::vector<std::vector<ObjectPtr>> object_paths_from(const ObjectPtr &obj) {
  return paths_from(obj, [](const ObjectPtr &sub_obj) { return sub_obj->getReferences<AudioObject>(); });
}

static std::vector<std::vector<PackFmtPointer>> pack_paths_from(const PackFmtPointer &pack) {
  return paths_from(pack, [](const PackFmtPointer &sub_pack) { return sub_pack->getReferences<AudioPackFormat>(); });
}

static void select_object_paths(ItemSelectionState state, const NextCB &next_cb) {
  for (const auto &root_object : select_root_objects(state))
    for (auto &object_path : object_paths_from(root_object)) {
      state.audioObjects = std::move(object_path);
      next_cb(state);
    }
}

static void select_programme_content_objects_default(ItemSelectionState state, const ProgrammePtr &audioProgramme,
                                                     const NextCB &next_cb) {
  if (state.adm->getElements<AudioProgramme>() || state.adm->getElements<AudioContent>()) {
    state = select_programme(state, audioProgramme);

    select_content(std::move(state),
                   [&next_cb](ItemSelectionState state_c) { select_object_paths(std::move(state_c), next_cb); });
  } else
    next_cb(std::move(state));
}

struct SelectStartVisitor {
  ItemSelectionState state;
  const SelectionOptions &options;
  const NextCB &next_cb;

  void operator()(const DefaultStart &) {
    select_programme_content_objects_default(std::move(state), nullptr, next_cb);
  }

  void operator()(const std::shared_ptr<adm::AudioProgramme> &programme) {
    select_programme_content_objects_default(std::move(state), programme, next_cb);
  }

  void operator()(const ContentStart &contents) {
    for (const auto &content : contents) {
      state.audioContent = content;
      select_object_paths(state, next_cb);
    }
  }

  void operator()(const ObjectStart &objects) {
    for (auto &object : objects) {
      for (auto &object_path : object_paths_from(object)) {
        state.audioObjects = std::move(object_path);
        next_cb(state);
      }
    }
  }
};

static void select_programme_content_objects(ItemSelectionState state, const SelectionOptions &options,
                                             const NextCB &next_cb) {
  std::visit(SelectStartVisitor{std::move(state), options, next_cb}, options.start);
}

struct AllocationTrackUID : public AllocationTrack {
  TrackUIDPointer track;

  AllocationTrackUID(ChannelFmtPointer channel_format_, PackFmtPointer pack_format_, TrackUIDPointer track_) noexcept
      : AllocationTrack(channel_format_, pack_format_), track(track_) {}
  virtual ~AllocationTrackUID() {}
};

class PackAllocator {
 public:
  PackAllocator(const DocumentPtr &document) {
    for (const PackFmtPointer &pack : document->getElements<AudioPackFormat>()) {
      auto alloc_pack = std::make_shared<AllocationPack>();
      alloc_pack->root_pack = pack;
      auto pack_paths = pack_paths_from(pack);
      for (const auto &pack_path : pack_paths) {
        const PackFmtPointer &last_pack = pack_path.at(pack_path.size() - 1);
        for (const ChannelFmtPointer &channel : last_pack->getReferences<AudioChannelFormat>()) {
          AllocationChannel alloc_channel;
          alloc_channel.channel_format = channel;
          alloc_channel.pack_formats = pack_path;
          alloc_pack->channels.push_back(std::move(alloc_channel));
        }
      }
      packs.push_back(std::move(alloc_pack));
    }
  }

  void select_pack_mapping(ItemSelectionState state, const NextCB &next_cb) {
    std::optional<std::vector<PackFmtPointer>> pack_refs;
    std::vector<TrackUIDPointer> tracks;
    size_t num_silent_tracks = 0;

    std::string error_context;

    if (state.audioObject()) {
      auto obj_pack_refs = state.audioObject()->getReferences<AudioPackFormat>();
      pack_refs = std::vector<PackFmtPointer>(obj_pack_refs.begin(), obj_pack_refs.end());

      for (auto &track : state.audioObject()->getReferences<AudioTrackUid>()) {
        if (track->isSilent())
          num_silent_tracks++;
        else
          tracks.push_back(track);
      }

      error_context = "in " + adm::formatId(state.audioObject()->get<adm::AudioObjectId>()) + ": ";
    } else {
      auto track_refs = state.adm->getElements<AudioTrackUid>();
      tracks = std::vector<TrackUIDPointer>(track_refs.begin(), track_refs.end());
      error_context = "in CHNA-only file: ";
    }

    std::vector<Ref<AllocationTrack>> alloc_tracks;

    for (const auto &track : tracks) {
      auto channel = channel_format_for_track_uid(track);

      alloc_tracks.push_back(std::make_shared<AllocationTrackUID>(channel_format_for_track_uid(track),
                                                                  track->getReference<AudioPackFormat>(), track));
    }

    auto allocations = allocate_packs(packs, alloc_tracks, pack_refs, num_silent_tracks);

    if (allocations.size() > 1)
      throw ItemSelectionError(error_context +
                               "found more than one solution when assigning packs and channels to tracks");
    else if (allocations.size() == 0)
      throw ItemSelectionError(error_context + "found no solutions when assigning packs and channels to tracks");
    else {
      const auto &allocation = allocations.at(0);

      for (const auto &pack_allocation : allocation) {
        ItemSelectionState new_state = state;
        new_state.audioPackFormat = pack_allocation.pack->root_pack;

        new_state.channel_allocation.clear();
        for (size_t channel_idx = 0; channel_idx < pack_allocation.pack->channels.size(); channel_idx++) {
          const auto &alloc_channel = pack_allocation.pack->channels.at(channel_idx);
          const auto &alloc_track = pack_allocation.allocation.at(channel_idx);

          TrackSpec track_spec;

          if (alloc_track) {
            const auto alloc_track_uid = std::dynamic_pointer_cast<const AllocationTrackUID>(alloc_track);
            framework::always_assert(static_cast<bool>(alloc_track_uid), "expected AllocationTrackUID");
            track_spec = DirectTrackSpec{alloc_track_uid->track};
          } else
            track_spec = SilentTrackSpec{};

          new_state.channel_allocation[alloc_channel.channel_format] = track_spec;
        }

        next_cb(std::move(new_state));
      }
    }
  }

 private:
  std::vector<Ref<AllocationPack>> packs;
};

static void select_single_channel(const ItemSelectionState &state, const NextCB &next_cb) {
  // TODO: pull these two loops out
  for (const auto &pack_path : pack_paths_from(state.audioPackFormat)) {
    const PackFmtPointer &last_pack = pack_path.at(pack_path.size() - 1);
    for (const auto &channel : last_pack->getReferences<AudioChannelFormat>()) {
      ItemSelectionState next_state = state;
      next_state.audioPackFormat_path = pack_path;
      next_state.audioChannelFormat = channel;
      next_state.track_spec = state.channel_allocation.at(channel);

      next_cb(std::move(next_state));
    }
  }
}

static ChannelFmtPointer channel_format_for_track_uid_indirect(const TrackUIDPointer &track) {
  auto trackFormat = track->getReference<AudioTrackFormat>();
  assert(trackFormat);

  auto streamFormat = trackFormat->getReference<AudioStreamFormat>();
  framework::always_assert(static_cast<bool>(streamFormat), "found audioTrackFormat without audioStreamFormatRef");

  auto channelFormat = streamFormat->getReference<AudioChannelFormat>();
  framework::always_assert(static_cast<bool>(streamFormat), "found audioStreamFormat without audioChannelFormatRef");

  return channelFormat;
}

ChannelFmtPointer channel_format_for_track_uid(const TrackUIDPointer &track) {
  ChannelFmtPointer channelFormat_direct = track->getReference<AudioChannelFormat>();

  auto trackFormat = track->getReference<AudioTrackFormat>();
  ChannelFmtPointer channelFormat_indirect;
  if (trackFormat) channelFormat_indirect = channel_format_for_track_uid_indirect(track);

  if (channelFormat_indirect && channelFormat_direct) {
    if (channelFormat_indirect == channelFormat_direct)
      return channelFormat_indirect;
    else
      throw ItemSelectionError(
          "audioTrackUID has both audioChannelFormat and audioTrackFormat "
          "reference which point to different audioChannelFormats");
  } else if (channelFormat_indirect)
    return channelFormat_indirect;
  else if (channelFormat_direct)
    return channelFormat_direct;
  else
    throw ItemSelectionError("audioTrackUID has neither an audioTrackFormat or audioChannelFormat reference");
}

static ADMPath get_adm_path(const ItemSelectionState &state) {
  ADMPath path;
  path.audioProgramme = state.audioProgramme;
  path.audioContent = state.audioContent;
  path.audioObjects = state.audioObjects;
  path.audioPackFormats = state.audioPackFormat_path;
  path.audioChannelFormat = state.audioChannelFormat;

  return path;
}

static void get_rendering_items_Objects(const ItemSelectionState &state, const RenderingItemCB &cb) {
  select_single_channel(state, [&cb](const ItemSelectionState &channel_state) {
    auto rendering_item = std::make_shared<ObjectRenderingItem>();

    rendering_item->adm_path = get_adm_path(channel_state);
    rendering_item->track_spec = channel_state.track_spec;

    cb(rendering_item);
  });
}

static void get_rendering_items_DirectSpeakers(const ItemSelectionState &state, const RenderingItemCB &cb) {
  select_single_channel(state, [&cb](const ItemSelectionState &channel_state) {
    auto rendering_item = std::make_shared<DirectSpeakersRenderingItem>();

    rendering_item->adm_path = get_adm_path(channel_state);
    rendering_item->track_spec = channel_state.track_spec;

    cb(rendering_item);
  });
}

// HOA-specific utilities
namespace hoa {

template <typename T, typename Element>
std::optional<T> get_parameter(const Element &el) {
  // we will deal with defaults ourselves after merging parameters from
  // different elements, as a default parameter should not conflict with a real
  // parameter
  if (el.template has<T>() && !el.template isDefault<T>())
    return {el.template get<T>()};
  else
    return {};
}

template <typename T>
std::optional<T> get_block_parameter(const ItemSelectionState &state) {
  const auto blocks = state.audioChannelFormat->getElements<AudioBlockFormatHoa>();
  if (blocks.size() != 1) throw ItemSelectionError("HOA audioChannelFormats must have exactly 1 audioBlockFormat");
  return get_parameter<T>(blocks[0]);
}

template <typename T>
void update_parameter(std::optional<T> &value, std::optional<T> value_from_el) {
  if (value && value_from_el && *value != *value_from_el) throw ItemSelectionError("incompatible parameters found");
  if (value_from_el) value = value_from_el;
}

template <typename T>
std::optional<T> get_path_parameter(const ItemSelectionState &state) {
  std::optional<T> value;

  for (const auto &pack : state.audioPackFormat_path) {
    auto hoa_pack = std::dynamic_pointer_cast<AudioPackFormatHoa>(pack);
    update_parameter(value, get_parameter<T>(*hoa_pack));
  }
  update_parameter(value, get_block_parameter<T>(state));

  return value;
}

// given an optional containing a namedtype, extract the value contained in the namedtype
template <typename T>
std::optional<typename T::value_type> unwrap_optional(const std::optional<T> &value) {
  if (value)
    return value->get();
  else
    return {};
}

static std::optional<double> unwrap_nfcRefDist(const std::optional<NfcRefDist> &value) {
  if (!value)
    return {};
  else if (value->get() == 0.0)  // check for literal zero in the file -- no tolerance required
    return {};
  else
    return value->get();
}

template <typename T>
T get_single_parameter(const std::vector<T> &values) {
  framework::always_assert(values.size() > 0, "HOA pack has no channels");
  for (size_t i = 1; i < values.size(); i++)
    if (values[i] != values[0]) throw ItemSelectionError("incompatible parameters found");
  return values[0];
}

static void get_rendering_items_HOA(const ItemSelectionState &state, const RenderingItemCB &cb) {
  auto rendering_item = std::make_shared<HOARenderingItem>();

  // assume HOA channels only have one block for now
  rendering_item->type_metadata.resize(1);
  auto &type_metadata = rendering_item->type_metadata.at(0);

  std::vector<std::string> normalization_values;
  std::vector<std::optional<Time>> rtime_values;
  std::vector<std::optional<Time>> duration_values;
  std::vector<std::optional<double>> nfcRefDist_values;
  std::vector<bool> screenRef_values;

  select_single_channel(
      state, [&rendering_item, &normalization_values, &rtime_values, &duration_values, &nfcRefDist_values,
              &screenRef_values, &type_metadata](const ItemSelectionState &channel_state) {
        normalization_values.push_back(
            get_path_parameter<Normalization>(channel_state).value_or(Normalization{"SN3D"}).get());
        rtime_values.push_back(unwrap_optional(get_block_parameter<Rtime>(channel_state)));
        duration_values.push_back(unwrap_optional(get_block_parameter<Duration>(channel_state)));
        nfcRefDist_values.push_back(unwrap_nfcRefDist(get_path_parameter<NfcRefDist>(channel_state)));
        screenRef_values.push_back(unwrap_optional(get_path_parameter<ScreenRef>(channel_state)).value_or(false));

        rendering_item->tracks.push_back(channel_state.track_spec);
        rendering_item->adm_paths.push_back(get_adm_path(channel_state));

        std::optional<Order> order = get_block_parameter<Order>(channel_state);
        if (!order) throw ItemSelectionError("HOA audioBlockFormats must have an order parameter");
        type_metadata.orders.push_back(order->get());

        std::optional<Degree> degree = get_block_parameter<Degree>(channel_state);
        if (!degree) throw ItemSelectionError("HOA audioBlockFormats must have a degree parameter");
        type_metadata.degrees.push_back(degree->get());
      });

  type_metadata.normalization = get_single_parameter(normalization_values);
  type_metadata.rtime = get_single_parameter(rtime_values);
  type_metadata.duration = get_single_parameter(duration_values);
  type_metadata.nfcRefDist = get_single_parameter(nfcRefDist_values);
  type_metadata.screenRef = get_single_parameter(screenRef_values);

  cb(rendering_item);
}
}  // namespace hoa

static void get_rendering_items(const ItemSelectionState &state, const RenderingItemCB &cb) {
  adm::TypeDescriptor pack_type = state.audioPackFormat->get<TypeDescriptor>();
  if (pack_type == adm::TypeDefinition::OBJECTS) {
    get_rendering_items_Objects(state, cb);
  } else if (pack_type == adm::TypeDefinition::DIRECT_SPEAKERS) {
    get_rendering_items_DirectSpeakers(state, cb);
  } else if (pack_type == adm::TypeDefinition::HOA) {
    hoa::get_rendering_items_HOA(state, cb);
  } else {
    throw ItemSelectionError("unsupported type " + formatTypeDefinition(pack_type));
  }
}

SelectionOptions::SelectionOptions(SelectionStart start_) : start(std::move(start_)) {}

SelectionResult select_items(const std::shared_ptr<adm::Document> &doc, const SelectionOptions &options) {
  SelectionResult result;

  PackAllocator pack_allocator{doc};

  ItemSelectionState initial_state;
  initial_state.adm = doc;
  initial_state.warn = [&result](const std::string &warning) { result.warnings.push_back(warning); };

  select_programme_content_objects(
      initial_state, options, [&pack_allocator, &result](const ItemSelectionState &state_pco) {
        pack_allocator.select_pack_mapping(state_pco, [&result](const ItemSelectionState &state_pack) {
          get_rendering_items(state_pack,
                              [&result](std::shared_ptr<RenderingItem> ri) { result.items.push_back(std::move(ri)); });
        });
      });

  return result;
}

}  // namespace eat::render
