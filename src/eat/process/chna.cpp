#include "eat/process/chna.hpp"

#include <adm/document.hpp>
#include <bw64/chunks.hpp>
#include <type_traits>

// work around https://github.com/ebu/libbw64/issues/36
#include <memory>

namespace eat::process {

template <typename T>
void setUidReference(adm::Document &doc, adm::AudioTrackUid &uid, T elementId) {
  auto element = doc.lookup(elementId);
  if (element) {
    using ElementT = std::remove_cvref_t<decltype(*element)>;
    using ElementIdT = typename ElementT::id_type;
    auto existing_ref = uid.getReference<ElementT>();

    if (!existing_ref)
      uid.setReference(element);
    else {
      if (existing_ref != element)
        throw std::runtime_error("in CHNA " + adm::formatId(uid.get<adm::AudioTrackUidId>()) + " refers to " +
                                 adm::formatId(elementId) + ", but in AXML it refers to " +
                                 adm::formatId(existing_ref->template get<ElementIdT>()));
    }
  } else
    throw std::runtime_error("could not find element referenced from CHNA: " + adm::formatId(elementId));
}

void load_chna(adm::Document &document, channel_map_t &channel_map, const bw64::ChnaChunk &chna) {
  for (auto &id : chna.audioIds()) {
    auto uidId = adm::parseAudioTrackUidId(id.uid());

    auto admUid = document.lookup(uidId);
    if (!admUid) admUid = adm::AudioTrackUid::create(uidId);

    if (id.trackRef().rfind("AT_", 0) == 0) {
      auto trackFormatId = adm::parseAudioTrackFormatId(id.trackRef());
      setUidReference(document, *admUid, trackFormatId);
    } else if (id.trackRef().rfind("AC_", 0) == 0) {
      auto audioChannelFormatId = adm::parseAudioChannelFormatId(
          id.trackRef().substr(0, 11));  // counter portion does not apply to audioChannelFormatId
      setUidReference(document, *admUid, audioChannelFormatId);
    } else
      throw std::runtime_error("unexpected track ID format in CHNA: " + id.trackRef());

    auto audioPackFormatId = adm::parseAudioPackFormatId(id.packRef());
    setUidReference(document, *admUid, audioPackFormatId);

    channel_map[uidId] = id.trackIndex() - 1;
  }
}

bw64::ChnaChunk make_chna(const adm::Document &document, const channel_map_t &channel_map) {
  std::vector<bw64::AudioId> audio_ids;

  for (auto &id : document.getElements<adm::AudioTrackUid>()) {
    if (id->isSilent()) continue;

    uint16_t trackIndex = channel_map.at(id->get<adm::AudioTrackUidId>());
    std::string uid = adm::formatId(id->get<adm::AudioTrackUidId>());

    std::string trackRef;
    if (id->getReference<adm::AudioTrackFormat>())
      trackRef = adm::formatId(id->getReference<adm::AudioTrackFormat>()->get<adm::AudioTrackFormatId>());
    else if (id->getReference<adm::AudioChannelFormat>())
      trackRef = adm::formatId(id->getReference<adm::AudioChannelFormat>()->get<adm::AudioChannelFormatId>()) + "_00";
    else
      throw std::runtime_error(
          "when making CHNA chunk, found audioTrackUID without "
          "audioChannelFormat or audioTrackFormat reference");

    if (!id->getReference<adm::AudioPackFormat>())
      throw std::runtime_error("when making CHNA chunk, found audioTrackUID without audioPackFormat reference");
    std::string packRef = adm::formatId(id->getReference<adm::AudioPackFormat>()->get<adm::AudioPackFormatId>());

    audio_ids.emplace_back(trackIndex + 1, uid, trackRef, packRef);
  }

  return {audio_ids};
}

}  // namespace eat::process
