#include "eat/process/profile_conversion_misc.hpp"

#include <adm/document.hpp>
#include <adm/utilities/id_assignment.hpp>
#include <set>
#include <vector>

#include "../render/rendering_items_internals.hpp"
#include "eat/process/adm_bw64.hpp"
#include "eat/process/block.hpp"
#include "eat/process/time_utils.hpp"
#include "eat/render/rendering_items.hpp"

using namespace eat::framework;

namespace eat::process {

struct ToAdmProfile {
  adm::Profile operator()(profiles::ITUEmissionProfile &p) {
    return adm::Profile{adm::ProfileValue{"ITU-R BS.[ADM-NGA-Emission]-0"},
                        adm::ProfileName{"AdvSS Emission ADM and S-ADM Profile"}, adm::ProfileVersion{"1"},
                        adm::ProfileLevel{std::to_string(p.level())}};
  }
};

class SetProfiles : public FunctionalAtomicProcess {
 public:
  SetProfiles(const std::string &name, const std::vector<profiles::Profile> &profiles_)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")),
        profiles(profiles_) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    std::vector<adm::Profile> adm_profiles;
    for (auto &profile : profiles) adm_profiles.push_back(std::visit(ToAdmProfile(), profile));

    doc->set(adm::ProfileList{std::move(adm_profiles)});

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
  std::vector<profiles::Profile> profiles;
};

ProcessPtr make_set_profiles(const std::string &name, const std::vector<profiles::Profile> &profiles) {
  return std::make_shared<SetProfiles>(name, profiles);
}

struct SetDefaultPositionValuesVisitor {
  adm::Position operator()(adm::SphericalPosition position) {
    position.set(position.get<adm::Distance>());
    return position;
  }

  adm::Position operator()(adm::CartesianPosition position) {
    position.set(position.get<adm::Z>());
    return position;
  }

  adm::SpeakerPosition operator()(adm::SphericalSpeakerPosition position) {
    position.set(position.get<adm::Distance>());
    return position;
  }

  adm::SpeakerPosition operator()(adm::CartesianSpeakerPosition position) {
    position.set(position.get<adm::Z>());
    return position;
  }
};

class SetPositionDefaults : public FunctionalAtomicProcess {
 public:
  SetPositionDefaults(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    for (auto &channel : doc->getElements<adm::AudioChannelFormat>()) {
      for (auto &block : channel->getElements<adm::AudioBlockFormatObjects>())
        block.set(boost::apply_visitor(SetDefaultPositionValuesVisitor{}, block.get<adm::Position>()));

      for (auto &block : channel->getElements<adm::AudioBlockFormatDirectSpeakers>()) {
        // TODO: libadm is missing AudioBlockFormatDirectSpeakers::get<SpeakerPosition>()
        adm::SpeakerPosition pos = block.has<adm::CartesianSpeakerPosition>()
                                       ? adm::SpeakerPosition{block.get<adm::CartesianSpeakerPosition>()}
                                       : adm::SpeakerPosition{block.get<adm::SphericalSpeakerPosition>()};
        block.set(boost::apply_visitor(SetDefaultPositionValuesVisitor{}, pos));
      }
    }

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
};

framework::ProcessPtr make_set_position_defaults(const std::string &name) {
  return std::make_shared<SetPositionDefaults>(name);
}

class RemoveSilentATUData : public FunctionalAtomicProcess {
 public:
  RemoveSilentATUData(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")),
        out_add_silent(add_out_port<DataPort<bool>>("out_add_silent")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    out_add_silent->set_value(false);
    size_t silent_track_idx = adm.channel_map.size();

    for (auto &object : doc->getElements<adm::AudioObject>()) {
      // only process objects with silent track refs
      bool any_silent = false;
      for (auto &atu : object->getReferences<adm::AudioTrackUid>())
        if (atu->isSilent()) any_silent = true;
      if (!any_silent) continue;

      auto result = render::select_items(doc, {render::ObjectStart{object}});

      // remove all ATU refs (re-added below)
      auto atu_range = object->getReferences<adm::AudioTrackUid>();
      std::vector<std::shared_ptr<adm::AudioTrackUid>> atus(atu_range.begin(), atu_range.end());
      object->clearReferences<adm::AudioTrackUid>();

      // figure out if we should use channel or track refs in any new ATUs
      bool use_channel_ref = false;
      for (auto &atu : atus)
        if (atu->getReference<adm::AudioChannelFormat>()) use_channel_ref = true;

      // for each channel in the item selection result, if it has an ATU,
      // re-add it, or make a new ATU associated with the silent track that
      // references the channel
      for (auto &item : result.items) {
        for_each_channel(item, [&](const auto &adm_path, const auto &track_spec) {
          // XXX: don't process tracks of sub-objects
          // we really shouldn't be using item selection directly here...
          framework::always_assert(adm_path.audioObjects.size(), "expected path to contain objects");
          if (adm_path.audioObjects.back() != object) return;

          if (std::holds_alternative<render::DirectTrackSpec>(track_spec))
            object->addReference(std::get<render::DirectTrackSpec>(track_spec).track);
          else if (std::holds_alternative<render::SilentTrackSpec>(track_spec)) {
            out_add_silent->set_value(true);

            auto atu = adm::AudioTrackUid::create();
            doc->add(atu);
            object->addReference(atu);

            atu->setReference(adm_path.audioPackFormats.at(0));

            if (use_channel_ref)
              atu->setReference(adm_path.audioChannelFormat);
            else {
              auto atf = get_atf_for_acf(doc, adm_path.audioChannelFormat);
              atu->setReference(atf);
            }

            adm.channel_map.emplace(atu->get<adm::AudioTrackUidId>(), silent_track_idx);
          } else
            // if we were to support matrix types, this could happen
            //
            // using RI selection here is a bit of a cheat to avoid having to
            // do pack allocation ourselves; the right solution would be to
            // re-use the pack allocation code, but skip all the special
            // matrix structures in the initialisation (as the code was when
            // this was added)
            throw std::runtime_error("don't know how to deal with other track spec types");
        });
      }
    }

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  // call cb with each pair of ADMPath and TrackSpec in item, to work with both
  // mono and multi-channel formats
  template <typename CB>
  void for_each_channel(const std::shared_ptr<render::RenderingItem> &item, CB cb) {
    if (auto mono_item = std::dynamic_pointer_cast<render::MonoRenderingItem>(item)) {
      cb(mono_item->adm_path, mono_item->track_spec);
    } else if (auto hoa_item = std::dynamic_pointer_cast<render::HOARenderingItem>(item)) {
      for (size_t i = 0; i < hoa_item->tracks.size(); i++) cb(hoa_item->adm_paths.at(i), hoa_item->tracks.at(i));
    } else
      throw std::runtime_error("don't know how to deal with other RI types");
  }

  // find or make an audioTrackFormat that points at a givenaudioChannelFormat
  std::shared_ptr<adm::AudioTrackFormat> get_atf_for_acf(const std::shared_ptr<adm::Document> &doc,
                                                         const std::shared_ptr<adm::AudioChannelFormat> &acf) {
    // find an audioStreamFormat that references acf
    for (auto &existing_asf : doc->getElements<adm::AudioStreamFormat>()) {
      auto ch_ref = existing_asf->getReference<adm::AudioChannelFormat>();
      if (ch_ref == acf) {
        // ... then find an audioTrackFormat that references that audioStreamFormat
        for (auto &existing_atf : doc->getElements<adm::AudioTrackFormat>()) {
          auto sf_ref = existing_atf->getReference<adm::AudioStreamFormat>();
          if (sf_ref == existing_asf) return existing_atf;
        }
      }
    }

    // otherwise make a new one
    std::string name = acf->get<adm::AudioChannelFormatName>().get();

    auto asf = adm::AudioStreamFormat::create(adm::AudioStreamFormatName(name), adm::FormatDefinition::PCM);
    doc->add(asf);

    auto atf = adm::AudioTrackFormat::create(adm::AudioTrackFormatName(name), adm::FormatDefinition::PCM);
    doc->add(atf);

    atf->setReference(asf);
    asf->setReference(acf);

    return atf;
  }

  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
  DataPortPtr<bool> out_add_silent;
};

/// append a silent track to some samples if in_add_silent is set
class AddSilentTrack : public StreamingAtomicProcess {
 public:
  AddSilentTrack(const std::string &name)
      : StreamingAtomicProcess(name),
        in_samples(add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples")),
        out_samples(add_out_port<StreamPort<InterleavedBlockPtr>>("out_samples")),
        in_add_silent(add_in_port<DataPort<bool>>("in_add_silent")) {}

  void process() override {
    while (in_samples->available()) {
      if (in_add_silent->get_value()) {
        auto in_block = in_samples->pop().read();
        auto &in_description = in_block->info();

        auto out_description = in_description;
        out_description.channel_count++;

        auto out_block = std::make_shared<InterleavedSampleBlock>(out_description);

        for (size_t sample_i = 0; sample_i < out_description.sample_count; sample_i++)
          for (size_t channel_i = 0; channel_i < in_description.channel_count; channel_i++) {
            out_block->sample(channel_i, sample_i) = in_block->sample(channel_i, sample_i);
          }

        out_samples->push(std::move(out_block));
      } else {
        out_samples->push(in_samples->pop());
      }
    }
    if (in_samples->eof()) out_samples->close();
  }

 private:
  StreamPortPtr<InterleavedBlockPtr> in_samples;
  StreamPortPtr<InterleavedBlockPtr> out_samples;
  DataPortPtr<bool> in_add_silent;
};

/// combination of RemoveSilentATUData and AddSilentTrack
class RemoveSilentATU : public CompositeProcess {
 public:
  RemoveSilentATU(const std::string &name) : CompositeProcess(name) {
    auto in_samples = add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples");
    auto out_samples = add_out_port<StreamPort<InterleavedBlockPtr>>("out_samples");

    auto in_axml = add_in_port<DataPort<ADMData>>("in_axml");
    auto out_axml = add_out_port<DataPort<ADMData>>("out_axml");

    auto remove_silent_data = add_process<RemoveSilentATUData>("remove_silent_data");
    auto add_silent_track = add_process<AddSilentTrack>("add_silent_track");

    connect(in_axml, remove_silent_data->get_in_port("in_axml"));
    connect(remove_silent_data->get_out_port("out_axml"), out_axml);
    connect(remove_silent_data->get_out_port("out_add_silent"), add_silent_track->get_in_port("in_add_silent"));

    connect(in_samples, add_silent_track->get_in_port("in_samples"));
    connect(add_silent_track->get_out_port("out_samples"), out_samples);
  }
};

ProcessPtr make_remove_silent_atu(const std::string &name) { return std::make_shared<RemoveSilentATU>(name); }

using ChannelVec = std::vector<std::shared_ptr<adm::AudioChannelFormat>>;
using ObjectVec = std::vector<std::shared_ptr<adm::AudioObject>>;

using Group = std::pair<ChannelVec, ObjectVec>;
using Groups = std::vector<Group>;

template <typename A, typename B>
void add_to_map_set(std::map<A, std::set<B>> &map, const A &a, B b) {
  auto [it, inserted] = map.try_emplace(a, std::set<B>{});
  it->second.emplace(std::move(b));
}

using ObjectOrChannel = std::variant<std::shared_ptr<adm::AudioObject>, std::shared_ptr<adm::AudioChannelFormat>>;

static Groups group_objects_and_channels(adm::Document &doc) {
  std::map<ObjectOrChannel, std::set<ObjectOrChannel>> next;

  for (auto &audioObject : doc.getElements<adm::AudioObject>()) {
    for (auto &atu : audioObject->getReferences<adm::AudioTrackUid>()) {
      if (atu->isSilent()) continue;

      auto acf = render::channel_format_for_track_uid(atu);
      add_to_map_set(next, ObjectOrChannel{acf}, ObjectOrChannel{audioObject});
      add_to_map_set(next, ObjectOrChannel{audioObject}, ObjectOrChannel{acf});
    }
  }

  std::set<ObjectOrChannel> remaining;
  for (auto &audioObject : doc.getElements<adm::AudioObject>()) remaining.insert(audioObject);
  for (auto &audioChannelFormat : doc.getElements<adm::AudioChannelFormat>()) remaining.insert(audioChannelFormat);

  Groups groups;
  std::vector<ObjectOrChannel> to_visit;

  while (!remaining.empty()) {
    to_visit.emplace_back(std::move(remaining.extract(remaining.begin()).value()));

    ChannelVec channels;
    ObjectVec objects;

    while (to_visit.size()) {
      auto node = std::move(to_visit.back());
      to_visit.pop_back();

      if (std::holds_alternative<std::shared_ptr<adm::AudioObject>>(node))
        objects.push_back(std::get<std::shared_ptr<adm::AudioObject>>(node));
      else
        channels.push_back(std::get<std::shared_ptr<adm::AudioChannelFormat>>(node));

      if (auto it = next.find(node); it != next.end()) {
        for (auto &next_node : it->second) {
          if (remaining.erase(next_node)) {
            to_visit.push_back(next_node);
          }
        }
      }
    }

    groups.emplace_back(std::move(channels), std::move(objects));
  }

  return groups;
}

template <typename CB>
void visit_all_blocks(adm::AudioChannelFormat &cf, CB cb) {
  for (auto &block : cf.getElements<adm::AudioBlockFormatDirectSpeakers>()) cb(block);
  for (auto &block : cf.getElements<adm::AudioBlockFormatMatrix>()) cb(block);
  for (auto &block : cf.getElements<adm::AudioBlockFormatObjects>()) cb(block);
  for (auto &block : cf.getElements<adm::AudioBlockFormatHoa>()) cb(block);
  for (auto &block : cf.getElements<adm::AudioBlockFormatBinaural>()) cb(block);
}

class RemoveObjectTimesDataSafe : public FunctionalAtomicProcess {
 public:
  RemoveObjectTimesDataSafe(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    // goal: remove rtimes and durations from objects when there are real
    // channel formats
    //
    // - two vague requirements (see process_group for more detail):
    //   - for each modified ACF, all audioObjects have the same start/duration
    //   - for each modified audioObject, all audioChannelFormats are modifiable
    //
    // - we should skip processing if either of these are not true, therefore
    //   we need to look at at groups of channels and object that are connected
    //   (in either direction)

    auto groups = group_objects_and_channels(*doc);

    for (auto &[channels, objects] : groups) {
      process_group(channels, objects);
    }

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  void process_group(const ChannelVec &channels, const ObjectVec &objects) {
    if (objects.size() == 0) return;  // nothing to fix

    // group will be modified if
    // - all channels are non-common-defs
    // - all objects have the same start time
    // - either
    //   - all audioObjects have the same duration
    //   - all channels have blocks with times/durations, and all object
    //     durations are after the last block end time for
    //     all objects (currently not handled)
    // - one of the objects has a start or duration
    //
    // there may be some cases where objects have durations that are
    // ineffective (e.g. start=0, duration > object len); these should be
    // detected and removed in another process

    for (auto &channel : channels) {
      if (adm::isCommonDefinitionsId(channel->get<adm::AudioChannelFormatId>())) return;
    }

    adm::AudioObject &first_obj = *objects.front();
    for (size_t i = 1; i < objects.size(); i++) {
      adm::AudioObject &obj = *objects.at(i);

      // objects have different start times
      if (first_obj.get<adm::Start>().get().asFractional().normalised() !=
          obj.get<adm::Start>().get().asFractional().normalised())
        return;

      // objects have different durations
      if (first_obj.has<adm::Duration>() != obj.has<adm::Duration>()) return;
      if (first_obj.has<adm::Duration>() && first_obj.get<adm::Duration>().get().asFractional().normalised() !=
                                                obj.get<adm::Duration>().get().asFractional().normalised())
        return;
    }

    bool some_object_has_time = false;
    for (auto &object : objects) {
      if (!object->isDefault<adm::Start>() || object->has<adm::Duration>()) some_object_has_time = true;
    }
    if (!some_object_has_time) return;

    for (auto &channel : channels) {
      visit_all_blocks(*channel, [&](auto &block) {
        if (!block.template isDefault<adm::Rtime>() && block.template has<adm::Duration>()) {
          // both rtime and duration -> update rtime
          block.set(adm::Rtime{time_add(block.template get<adm::Rtime>().get(), first_obj.get<adm::Start>().get())});
        } else if (block.template isDefault<adm::Rtime>() && !block.template has<adm::Duration>()) {
          // neither rtime and duration -> copy times from object
          block.set(adm::Rtime{first_obj.get<adm::Start>().get()});
          block.set(first_obj.get<adm::Duration>());
        } else
          throw std::runtime_error{"blocks must have both rtime and duration, or neither"};
      });
    }

    for (auto &object : objects) {
      object->unset<adm::Start>();
      object->unset<adm::Duration>();
    }
  }

  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
};

ProcessPtr make_remove_object_times_data_safe(const std::string &name) {
  return std::make_shared<RemoveObjectTimesDataSafe>(name);
}

class RemoveObjectTimesCommonUnsafe : public FunctionalAtomicProcess {
 public:
  RemoveObjectTimesCommonUnsafe(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    for (auto &object : doc->getElements<adm::AudioObject>()) process_object(object);

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  void process_object(const std::shared_ptr<adm::AudioObject> &object) {
    for (auto &atu : object->getReferences<adm::AudioTrackUid>()) {
      if (atu->isSilent()) continue;

      auto acf = render::channel_format_for_track_uid(atu);

      if (!adm::isCommonDefinitionsId(acf->get<adm::AudioChannelFormatId>())) return;
    }

    object->unset<adm::Start>();
    object->unset<adm::Duration>();
  }
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
};

ProcessPtr make_remove_object_times_common_unsafe(const std::string &name) {
  return std::make_shared<RemoveObjectTimesCommonUnsafe>(name);
}

class RemoveImportance : public FunctionalAtomicProcess {
 public:
  RemoveImportance(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    for (auto &ao : doc->getElements<adm::AudioObject>()) ao->unset<adm::Importance>();

    for (auto &apf : doc->getElements<adm::AudioPackFormat>()) apf->unset<adm::Importance>();

    for (auto &acf : doc->getElements<adm::AudioChannelFormat>())
      visit_all_blocks(*acf, [](auto &block) { block.template unset<adm::Importance>(); });

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
};

ProcessPtr make_remove_importance(const std::string &name) { return std::make_shared<RemoveImportance>(name); }

/// make programme/content/object structures compatible with the emission profile
///
/// the restrictions are:
/// - audioContents can only reference one audioObject
/// - only audioObjects containing Objects content can be nested
/// - there's an undefined "maximum nest level" of 2
///
/// the general approach is to recurse down the programme-content-object
/// structure with functions that take one element and return a list of new (or
/// existing) elements that it should be replaced with
///
/// each of these functions applies the rules itself (returning the current
/// depth with objects), deciding whether to keep the existing element
/// (replacing the references with those found by recursion, or to split the
/// element up in order to satisfy the nesting rules
///
/// because contents and objects can be referenced from multiple parents the
/// structure is a DAG rather than a tree. to handle this, each function caches
/// its results, so that each element is only processed once
class RewriteContentObjectsEmission : public FunctionalAtomicProcess {
 public:
  RewriteContentObjectsEmission(const std::string &name, int max_objects_depth_)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")),
        max_objects_depth(max_objects_depth_) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    doc = adm.document.move_or_copy();

    // populate program/content/object info, which is used because references are
    // removed by libadm when elements are removed

    for (auto &programme : doc->getElements<adm::AudioProgramme>()) {
      auto contents_range = programme->getReferences<adm::AudioContent>();
      programme_info.emplace(programme, ProgrammeInfo{Vec<ContentPtr>{contents_range.begin(), contents_range.end()}});
    }

    for (auto &content : doc->getElements<adm::AudioContent>()) {
      auto objects_range = content->getReferences<adm::AudioObject>();
      content_info.emplace(content, ContentInfo{Vec<ObjectPtr>{objects_range.begin(), objects_range.end()}});
    }

    for (auto &object : doc->getElements<adm::AudioObject>()) {
      auto objects_range = object->getReferences<adm::AudioObject>();
      object_info.emplace(object, ObjectInfo{Vec<ObjectPtr>{objects_range.begin(), objects_range.end()}});

      auto pack_range = object->getReferences<adm::AudioPackFormat>();
      auto atu_range = object->getReferences<adm::AudioTrackUid>();
      always_assert((objects_range.size() > 0) != (pack_range.size() > 0 || atu_range.size() > 0),
                    "object can have either content or object references");
    }

    for (auto &programme : doc->getElements<adm::AudioProgramme>()) {
      auto info = programme_info.at(programme);

      programme->clearReferences<adm::AudioContent>();
      for (auto &content : info.contents)
        for (auto &new_content : rewrite_content(content)) programme->addReference(new_content);
    }

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  using ProgrammePtr = std::shared_ptr<adm::AudioProgramme>;
  using ContentPtr = std::shared_ptr<adm::AudioContent>;
  using ObjectPtr = std::shared_ptr<adm::AudioObject>;
  using PackPtr = std::shared_ptr<adm::AudioPackFormat>;

  template <typename T>
  using Vec = std::vector<T>;

  // depth is the number of references between this and an object containing
  // only content (i.e. zero-based)
  using ObjectWithDepth = std::pair<ObjectPtr, int>;

  struct ObjectResult {
    Vec<ContentPtr> contents;
    Vec<ObjectWithDepth> objects;
  };

  ObjectResult rewrite_object(const ObjectPtr &object) {
    if (auto it = object_cache.find(object); it != object_cache.end()) return it->second;

    auto result = rewrite_object_uncached(object);
    object_cache.emplace(object, result);
    return result;
  }

  ObjectResult rewrite_object_uncached(const ObjectPtr &object) {
    ObjectInfo &info = object_info.at(object);

    ObjectResult result;
    if (info.objects.size() == 0) {
      // objects containing content are always fine
      result.objects.emplace_back(object, 0);
    } else {
      Vec<ObjectResult> sub_results;
      for (auto &sub_object : info.objects) sub_results.push_back(rewrite_object(sub_object));

      // for now, either keep it together, or break it up into objects or
      // contents
      // it can be kept in one object if:
      // - no sub-results are contents
      // - all sub-result objects have only Objects packs
      // - this object would not exceed the maximum nesting level
      //
      // if we decide to break it up, we don't need to consider the maximum
      // nesting level as each sub-result already meets the nesting level
      // requirement

      bool split = false;
      int object_depth = 0;

      for (auto &sub_result : sub_results) {
        if (sub_result.contents.size()) split = true;

        for (auto &[sub_object, sub_object_depth] : sub_result.objects) {
          auto sub_object_packs = sub_object->getReferences<adm::AudioPackFormat>();
          for (auto &sub_object_pack : sub_object_packs)
            if (sub_object_pack->get<adm::TypeDescriptor>() != adm::TypeDefinition::OBJECTS) split = true;

          // object_depth = std::max(sub_object_depth + 1, object_depth);
          if (sub_object_depth + 1 > object_depth) object_depth = sub_object_depth + 1;
        }
      }

      if (object_depth > max_objects_depth) split = true;

      if (split) {
        // TODO: we lose some information here -- any information in this
        // object which also applies to sub-objects should be copied to the
        // objects (/contents) below
        for (auto &sub_result : sub_results) {
          for (auto &sub_object : sub_result.objects) result.objects.push_back(sub_object);
          for (auto &sub_content : sub_result.contents) result.contents.push_back(sub_content);
        }
        doc->remove(object);

        always_assert(!object->isDefault<adm::Start>(), "removed audioObject with start");
        always_assert(object->has<adm::Duration>(), "removed audioObject with duration");
        always_assert(object->has<adm::DisableDucking>(), "removed audioObject with disableDucking");
        always_assert(object->getComplementaryObjects().size(), "removed audioObject with complementary objects");
        always_assert(object->has<adm::AudioObjectInteraction>(), "removed audioObject with interaction");
        always_assert(object->has<adm::Gain>(), "removed audioObject with gain");
        always_assert(object->has<adm::HeadLocked>(), "removed audioObject with headLocked");
        always_assert(object->has<adm::PositionOffset>(), "removed audioObject with positionOffset");
        always_assert(object->has<adm::Mute>(), "removed audioObject with mute");
      } else {
        object->clearReferences<adm::AudioObject>();
        for (auto &sub_result : sub_results)
          for (auto &[sub_object, sub_object_depth] : sub_result.objects) object->addReference(sub_object);

        result.objects.emplace_back(object, max_objects_depth);
      }
    }

    return result;
  }

  Vec<ContentPtr> rewrite_content(const ContentPtr &content) {
    if (auto it = content_cache.find(content); it != content_cache.end()) return it->second;

    auto result = rewrite_content_uncached(content);
    content_cache.emplace(content, result);
    return result;
  }

  Vec<ContentPtr> rewrite_content_uncached(const ContentPtr &content) {
    ContentInfo &info = content_info.at(content);

    Vec<ObjectResult> sub_results;
    for (auto &sub_object : info.objects) sub_results.push_back(rewrite_object(sub_object));

    // again, either split it or keep it as is
    // splitting is required if there is more than one sub-object, or any sub-contents

    size_t sub_object_count = 0;
    size_t sub_content_count = 0;
    for (auto &sub_result : sub_results) {
      sub_object_count += sub_result.objects.size();
      sub_content_count += sub_result.contents.size();
    }

    Vec<ContentPtr> results;
    if (sub_object_count > 1 || sub_content_count > 0) {
      // split
      // TODO: again there could be important info here that needs to be pushed down
      for (auto &sub_result : sub_results) {
        for (auto &sub_content : sub_result.contents) results.push_back(sub_content);

        for (auto &[sub_object, sub_object_depth] : sub_result.objects)
          results.push_back(object_to_content(sub_object));
      }
      doc->remove(content);
    } else {
      // keep
      content->clearReferences<adm::AudioObject>();
      for (auto &sub_result : sub_results)
        for (auto &[sub_object, sub_object_depth] : sub_result.objects) content->addReference(sub_object);

      results.push_back(content);
    }
    return results;
  }

  ContentPtr object_to_content(const ObjectPtr &object) {
    if (auto it = object_to_content_cache.find(object); it != object_to_content_cache.end()) return it->second;

    ContentPtr content = adm::AudioContent::create(adm::AudioContentName{object->get<adm::AudioObjectName>().get()},
                                                   object->get<adm::Labels>());

    if (object->has<adm::DialogueId>()) content->set(object->get<adm::DialogueId>());

    content->addReference(object);

    doc->add(content);

    object_to_content_cache.emplace(object, content);
    return content;
  }

  std::shared_ptr<adm::Document> doc;

  std::map<ContentPtr, Vec<ContentPtr>> content_cache;
  std::map<ObjectPtr, ObjectResult> object_cache;
  std::map<ObjectPtr, ContentPtr> object_to_content_cache;

  struct ProgrammeInfo {
    Vec<ContentPtr> contents;
  };

  std::map<ProgrammePtr, ProgrammeInfo> programme_info;

  struct ObjectInfo {
    Vec<ObjectPtr> objects;
  };

  std::map<ObjectPtr, ObjectInfo> object_info;

  struct ContentInfo {
    Vec<ObjectPtr> objects;
  };

  std::map<ContentPtr, ContentInfo> content_info;

  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;

  int max_objects_depth;
};

ProcessPtr make_rewrite_content_objects_emission(const std::string &name, int max_objects_depth) {
  return std::make_shared<RewriteContentObjectsEmission>(name, max_objects_depth);
}

class InferObjectInteract : public FunctionalAtomicProcess {
 public:
  InferObjectInteract(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    for (auto &object : doc->getElements<adm::AudioObject>()) {
      if (!object->has<adm::Interact>() || object->isDefault<adm::Interact>()) {
        object->set(adm::Interact{object->has<adm::AudioObjectInteraction>()});
      }
    }

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
};

ProcessPtr make_infer_object_interact(const std::string &name) { return std::make_shared<InferObjectInteract>(name); }

class SetContentDialogueDefault : public FunctionalAtomicProcess {
 public:
  SetContentDialogueDefault(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    for (auto &content : doc->getElements<adm::AudioContent>()) {
      if (!content->has<adm::DialogueId>()) content->set(adm::Dialogue::MIXED);
    }

    adm.document = std::move(doc);
    out_axml->set_value(std::move(adm));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
};

ProcessPtr make_set_content_dialogue_default(const std::string &name) {
  return std::make_shared<SetContentDialogueDefault>(name);
}

}  // namespace eat::process
