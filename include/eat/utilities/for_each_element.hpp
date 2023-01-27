#include <adm/elements_fwd.hpp>

namespace eat::utilities {

/// borrowed from https://github.com/ebu/libadm/pull/155
///
/// store a value for each top-level element type, with access by type
///
/// the value is given by the template T, so that get<AudioProgramme>()
/// returns a T<AudioProgramme>, for example
template <template <typename Element> typename T>
struct ForEachElement {
  /// get one of the stored values
  template <typename El>
  T<El> &get() {
    return getTag(typename El::tag{});
  }

  /// call f on each of the stored values
  template <typename F>
  void visit(F f) {
    f(programmes);
    f(contents);
    f(objects);
    f(packFormats);
    f(channelFormats);
    f(streamFormats);
    f(trackFormats);
    f(trackUids);
  }

 private:
  T<adm::AudioProgramme> &getTag(adm::AudioProgramme::tag) { return programmes; }
  T<adm::AudioContent> &getTag(adm::AudioContent::tag) { return contents; }
  T<adm::AudioObject> &getTag(adm::AudioObject::tag) { return objects; }
  T<adm::AudioPackFormat> &getTag(adm::AudioPackFormat::tag) { return packFormats; }
  T<adm::AudioChannelFormat> &getTag(adm::AudioChannelFormat::tag) { return channelFormats; }
  T<adm::AudioStreamFormat> &getTag(adm::AudioStreamFormat::tag) { return streamFormats; }
  T<adm::AudioTrackFormat> &getTag(adm::AudioTrackFormat::tag) { return trackFormats; }
  T<adm::AudioTrackUid> &getTag(adm::AudioTrackUid::tag) { return trackUids; }

  T<adm::AudioProgramme> programmes;
  T<adm::AudioContent> contents;
  T<adm::AudioObject> objects;
  T<adm::AudioPackFormat> packFormats;
  T<adm::AudioChannelFormat> channelFormats;
  T<adm::AudioStreamFormat> streamFormats;
  T<adm::AudioTrackFormat> trackFormats;
  T<adm::AudioTrackUid> trackUids;
};

}  // namespace eat::utilities
