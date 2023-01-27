#include "eat/process/remove_unused.hpp"

#include <adm/document.hpp>
#include <adm/utilities/id_assignment.hpp>
#include <optional>
#include <set>
#include <vector>

#include "eat/framework/process.hpp"
#include "eat/process/adm_bw64.hpp"
#include "eat/process/block.hpp"
#include "eat/process/channel_mapping.hpp"
#include "eat/utilities/for_each_element.hpp"
#include "eat/utilities/for_each_reference.hpp"

using namespace eat::framework;
using namespace eat::utilities;

namespace eat::process {

namespace detail {

template <typename Element>
using OneElementSet = std::set<std::shared_ptr<Element>>;

using ElementSet = ForEachElement<OneElementSet>;

/// add all elements referenced directly or indirectly from element to set
template <typename Element>
void add_referenced_elements(ElementSet &set, const std::shared_ptr<Element> &element) {
  auto &elements = set.get<Element>();
  if (elements.find(element) == elements.end()) {
    elements.insert(element);

    for_each_reference(element, [&set](auto referenced) { add_referenced_elements(set, referenced); });
  }
}

/// remove elements not in set from doc
template <typename Element>
void remove_not_in_set(const std::shared_ptr<adm::Document> &doc, const OneElementSet<Element> &set) {
  // iterate twice because removing would invalidate the range
  std::vector<std::shared_ptr<Element>> to_remove;

  for (const auto &element : doc->getElements<Element>())
    if (!adm::isCommonDefinitionsId(element->template get<typename Element::id_type>()) &&
        set.find(element) == set.end())
      to_remove.push_back(element);

  for (const auto &element : to_remove) doc->remove(element);
}

/// remove audioTrackUids not in document from channel_map
static void remove_atu_not_in_doc(const std::shared_ptr<adm::Document> &doc, channel_map_t &channel_map) {
  std::set<adm::AudioTrackUidId> known_ids;

  for (const auto &element : doc->getElements<adm::AudioTrackUid>())
    known_ids.insert(element->get<adm::AudioTrackUidId>());

  // iterate twice because removing would invalidate the map
  std::vector<adm::AudioTrackUidId> to_remove;
  for (const auto &pair : channel_map)
    if (known_ids.find(pair.first) == known_ids.end()) to_remove.push_back(pair.first);

  for (const auto &id : to_remove) channel_map.erase(id);
}

}  // namespace detail

/// remove elements from a document which are not used
class RemoveUnusedElements : public FunctionalAtomicProcess {
 public:
  RemoveUnusedElements(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    detail::ElementSet set;
    for (const auto &programme : doc->getElements<adm::AudioProgramme>())
      detail::add_referenced_elements(set, programme);

    set.visit([&doc](const auto &element_set) { detail::remove_not_in_set(doc, element_set); });

    detail::remove_atu_not_in_doc(doc, adm.channel_map);

    adm.document = std::move(doc);

    out_axml->set_value(std::move(adm));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
};

ProcessPtr make_remove_unused_elements(const std::string &name) { return std::make_shared<RemoveUnusedElements>(name); }

// to re-pack channels to remove unused ones, two separate processes are used,
// RepackChannels and ApplyChannelMapping. This is done so that downstream
// processes don't have to wait for audio to stream through the audio
// processing part before being able to use the ADM data

/// rewrite the channel indexes in the channel_mapping in ADMData so that used channels are adjacent
///
/// produces a ChannelMapping which should be used with ApplyChannelMapping
class RepackChannels : public FunctionalAtomicProcess {
 public:
  RepackChannels(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")),
        out_channel_mapping(add_out_port<DataPort<ChannelMapping>>("out_channel_mapping")) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());

    std::vector<std::optional<adm::AudioTrackUidId>> uids_for_channels;

    for (auto &[uid, channel] : adm.channel_map) {
      if (uids_for_channels.size() < channel + 1) uids_for_channels.resize(channel + 1);
      // XXX: each channel can be assigned to more than one ATU...
      uids_for_channels.at(channel) = uid;
    }

    ChannelMapping channel_mapping;

    size_t out_channel = 0;
    adm.channel_map.clear();
    for (size_t in_channel = 0; in_channel < uids_for_channels.size(); in_channel++) {
      auto &uid = uids_for_channels.at(in_channel);
      if (uid) {
        adm.channel_map.emplace(*uid, out_channel);
        assert(channel_mapping.size() == out_channel);
        channel_mapping.push_back(in_channel);
        out_channel++;
      }
    }

    out_axml->set_value(std::move(adm));
    out_channel_mapping->set_value(std::move(channel_mapping));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
  DataPortPtr<ChannelMapping> out_channel_mapping;
};

/// combination of RepackChannels and ApplyChannelMapping
class RemoveUnusedChannels : public CompositeProcess {
 public:
  RemoveUnusedChannels(const std::string &name) : CompositeProcess(name) {
    auto in_samples = add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples");
    auto out_samples = add_out_port<StreamPort<InterleavedBlockPtr>>("out_samples");

    auto in_axml = add_in_port<DataPort<ADMData>>("in_axml");
    auto out_axml = add_out_port<DataPort<ADMData>>("out_axml");

    auto repack = add_process<RepackChannels>("repack");
    auto apply = make_apply_channel_mapping("apply");
    register_process(apply);

    connect(in_axml, repack->get_in_port("in_axml"));
    connect(repack->get_out_port("out_axml"), out_axml);

    connect(repack->get_out_port("out_channel_mapping"), apply->get_in_port("in_channel_mapping"));

    connect(in_samples, apply->get_in_port("in_samples"));
    connect(apply->get_out_port("out_samples"), out_samples);
  }
};

/// combination of RemoveUnusedElements and RemoveUnusedChannels;
///
/// removes unused elements and the channels they references
class RemoveUnused : public CompositeProcess {
 public:
  RemoveUnused(const std::string &name) : CompositeProcess(name) {
    auto in_samples = add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples");
    auto out_samples = add_out_port<StreamPort<InterleavedBlockPtr>>("out_samples");

    auto in_axml = add_in_port<DataPort<ADMData>>("in_axml");
    auto out_axml = add_out_port<DataPort<ADMData>>("out_axml");

    auto remove_elements = add_process<RemoveUnusedElements>("remove_elements");
    auto remove_channels = add_process<RemoveUnusedChannels>("remove_channels");

    connect(in_axml, remove_elements->get_in_port("in_axml"));
    connect(remove_elements->get_out_port("out_axml"), remove_channels->get_in_port("in_axml"));
    connect(remove_channels->get_out_port("out_axml"), out_axml);

    connect(in_samples, remove_channels->get_in_port("in_samples"));
    connect(remove_channels->get_out_port("out_samples"), out_samples);
  }
};

ProcessPtr make_remove_unused(const std::string &name) { return std::make_shared<RemoveUnused>(name); }

}  // namespace eat::process
