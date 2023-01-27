//
// Created by Richard Bailey on 05/08/2022.
//
#include "eat/process/directspeaker_conversion.hpp"

#include <adm/adm.hpp>

namespace eat::process::detail {
using namespace adm;

namespace {
std::optional<detail::ConvertibleLayout> targetLayout(
    const adm::AudioPackFormat &pack, std::shared_ptr<adm::AudioTrackUid> uid,
    const std::map<std::string, std::vector<SpeakerData>> &layoutMap) {
  auto target = layoutMap.find(formatId(pack.get<adm::AudioPackFormatId>()));
  if (target != layoutMap.end()) {
    return ConvertibleLayout{{uid}, {target->second}};
  }
  return {};
}
}  // namespace

TargetLayoutMap find_target_layouts(Document &doc, const std::map<std::string, std::vector<SpeakerData>> &layoutMap) {
  TargetLayoutMap targetLayouts;
  for (auto const &uid : doc.getElements<adm::AudioTrackUid>()) {
    auto pack = uid->getReference<adm::AudioPackFormat>();
    // Have we already added this layout as a target for conversion?
    if (auto existingTarget = targetLayouts.find(pack); pack && existingTarget == targetLayouts.end()) {
      // If not, does a conversion from the pack referenced by this UID exist?
      if (auto target = targetLayout(*pack, uid, layoutMap)) {
        // Conversion exists but hasn't been added, so add layout target
        targetLayouts.insert({pack, *target});
      }
    } else {
      // Conversion target already exists so add this uid as a source
      existingTarget->second.uids.push_back(uid);
    }
  }
  return targetLayouts;
}

std::vector<SpeakerData> mapped_speaker_set(const TargetLayoutMap &layouts) {
  std::vector<SpeakerData> usedSpeakers;
  for (auto const &layout : layouts) {
    auto const &target_layout = layout.second.target_layout;
    std::copy(target_layout.begin(), target_layout.end(), std::back_inserter(usedSpeakers));
  }

  std::sort(usedSpeakers.begin(), usedSpeakers.end(),
            [](auto const &lhs, auto const &rhs) { return lhs.audioChannelFormatName < rhs.audioChannelFormatName; });

  auto it = std::unique(usedSpeakers.begin(), usedSpeakers.end(), [](auto const &lhs, auto const &rhs) {
    return lhs.audioChannelFormatName == rhs.audioChannelFormatName;
  });

  usedSpeakers.erase(it, usedSpeakers.end());

  return usedSpeakers;
}

namespace {
adm::AudioBlockFormatDirectSpeakers create_block_format(const SpeakerData &speaker) {
  adm::AudioBlockFormatDirectSpeakers block;
  block.add(adm::SpeakerLabel{speaker.speakerLabel});
  block.set(adm::CartesianSpeakerPosition{adm::X{speaker.pos.x}, adm::Y{speaker.pos.y}, adm::Z{speaker.pos.z}});
  return block;
}

DSChannel convert_speaker(SpeakerData const &speaker) {
  auto channel = adm::AudioChannelFormat::create(adm::AudioChannelFormatName(speaker.audioChannelFormatName),
                                                 adm::TypeDescriptor(adm::TypeDefinition::DIRECT_SPEAKERS));
  channel->add(create_block_format(speaker));
  auto track = adm::AudioTrackFormat::create(adm::AudioTrackFormatName("PCM_" + speaker.audioChannelFormatName),
                                             adm::FormatDefinition::PCM);
  auto stream = adm::AudioStreamFormat::create(adm::AudioStreamFormatName("PCM_" + speaker.audioChannelFormatName),
                                               adm::FormatDescriptor(adm::FormatDefinition::PCM));
  track->setReference(stream);
  stream->setReference(channel);
  return DSChannel{speaker.audioChannelFormatName, std::move(channel), std::move(track), std::move(stream)};
}
}  // namespace

std::vector<DSChannel> convert_speakers(const std::vector<SpeakerData> &speakerData) {
  std::vector<DSChannel> convertedChannels;
  convertedChannels.reserve(speakerData.size());
  std::transform(speakerData.begin(), speakerData.end(), std::back_inserter(convertedChannels), convert_speaker);
  return convertedChannels;
}

MappedPacks create_converted_packs(const TargetLayoutMap &layoutMap, const std::vector<DSChannel> &convertedChannels) {
  MappedPacks packs;
  auto i = 0;
  auto const channelComparator = [](DSChannel const &lhs, DSChannel const &rhs) {
    return lhs.audioChannelFormatName < rhs.audioChannelFormatName;
  };
  assert(std::is_sorted(convertedChannels.begin(), convertedChannels.end(), channelComparator));

  for (auto const &layout : layoutMap) {
    auto pack = adm::AudioPackFormat::create(adm::AudioPackFormatName("CustomPack_" + std::to_string(i)),
                                             adm::TypeDefinition::DIRECT_SPEAKERS);
    MappedPack mappedPack;
    mappedPack.target = pack;
    for (auto const &speaker : layout.second.target_layout) {
      // Quicker than std::find for a sorted range
      auto channel =
          std::lower_bound(convertedChannels.begin(), convertedChannels.end(),
                           DSChannel{speaker.audioChannelFormatName, nullptr, nullptr, nullptr}, channelComparator);
      assert(channel != convertedChannels.end() && channel->audioChannelFormatName == speaker.audioChannelFormatName);
      pack->addReference(channel->channel);
      mappedPack.elements.push_back(*channel);
    }
    packs.insert({layout.first, mappedPack});
  }
  return packs;
}

MappedUids create_converted_uids(adm::Document &doc, MappedPacks const &packs) {
  std::unordered_map<std::shared_ptr<adm::AudioTrackUid>, std::shared_ptr<adm::AudioTrackUid>> mappedUids;
  for (auto const &uid : doc.getElements<adm::AudioTrackUid>()) {
    auto source = uid->getReference<adm::AudioPackFormat>();
    auto target = packs.find(source);
    if (target != packs.end()) {
      auto targetPack = target->second.target;
      for (auto const &channel : target->second.elements) {
        auto targetUid = adm::AudioTrackUid::create();
        targetUid->setReference(targetPack);
        targetUid->setReference(channel.track);
        mappedUids[uid] = targetUid;
      }
    }
  }
  return mappedUids;
}

namespace {
std::vector<std::shared_ptr<adm::AudioTrackUid>> replacementUids(adm::AudioObject &object,
                                                                 MappedUids const &mapped_uids) {
  std::vector<std::shared_ptr<adm::AudioTrackUid>> replaced;
  for (auto const &uid : object.getReferences<adm::AudioTrackUid>()) {
    if (auto match = mapped_uids.find(uid); match != mapped_uids.end()) {
      replaced.push_back(match->second);
    }
  }
  return replaced;
}

void replaceObjectUidReferences(adm::AudioObject &object, MappedUids const &mapped_uids) {
  auto replacements = replacementUids(object, mapped_uids);
  if (!replacements.empty()) {
    object.clearReferences<adm::AudioTrackUid>();
    for (auto &ref : replacements) {
      object.addReference(ref);
    }
  }
}

void replaceDocumentUidReferences(adm::Document &doc, std::shared_ptr<adm::AudioTrackUid> const &oldUid,
                                  std::shared_ptr<adm::AudioTrackUid> const &newUid) {
  doc.remove(oldUid);
  // Adding the UID to the doc automatically adds the referenced elements
  doc.add(newUid);
}

void replaceChannelMappingUidreferences(channel_map_t &channelMap, std::shared_ptr<adm::AudioTrackUid> const &oldUid,
                                        std::shared_ptr<adm::AudioTrackUid> const &newUid) {
  auto currentChannelMapping = channelMap.find(oldUid->get<adm::AudioTrackUidId>());
  auto currentChannel = currentChannelMapping->second;
  channelMap.erase(currentChannelMapping);
  channelMap.insert({newUid->get<adm::AudioTrackUidId>(), currentChannel});
}

}  // namespace

void replace_layouts(adm::Document &doc, channel_map_t &channelMap, const MappedUids &mapped_uids) {
  for (auto &object : doc.getElements<AudioObject>()) {
    replaceObjectUidReferences(*object, mapped_uids);
  }
  for (auto const &[oldUid, newUid] : mapped_uids) {
    replaceDocumentUidReferences(doc, oldUid, newUid);
    replaceChannelMappingUidreferences(channelMap, oldUid, newUid);
  }
}

namespace {
bool isCommonDefinition(adm::AudioPackFormat const &pack) {
  return pack.get<adm::AudioPackFormatId>().get<adm::AudioPackFormatIdValue>().get() < 0x1000;
}

struct PackReplacement {
  std::shared_ptr<adm::AudioPackFormat> original;
  std::shared_ptr<adm::AudioPackFormat> updated;
};

std::vector<PackReplacement> packReplacements(adm::AudioObject &object, MappedPacks const &packMap) {
  auto packs = object.getReferences<adm::AudioPackFormat>();
  std::vector<PackReplacement> replacements;
  for (auto const &pack : packs) {
    if (isCommonDefinition(*pack)) {
      auto replacement = packMap.find(pack);
      if (replacement != packMap.end()) {
        replacements.push_back({pack, replacement->second.target});
      }
    }
  }
  return replacements;
}

}  // namespace

void replace_object_pack_references(adm::Document &doc, MappedPacks const &packMap) {
  auto objects = doc.getElements<adm::AudioObject>();
  for (auto const &object : objects) {
    auto replacements = packReplacements(*object, packMap);
    for (auto const &replacement : replacements) {
      object->removeReference(replacement.original);
      object->addReference(replacement.updated);
    }
  }
}

}  // namespace eat::process::detail
