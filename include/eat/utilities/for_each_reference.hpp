#include <adm/elements.hpp>
#include <type_traits>

namespace eat::utilities {

namespace detail {

/// type of references between objects
enum class ReferenceType {
  NONE,        /// no reference
  MULTIPLE,    /// multiple references with getReferences
  SINGLE,      /// a single reference with getReference
  WEAK_TRACK,  /// weak references using getAudioTrackFormatReferences
};

template <typename From, typename To>
struct ReferenceInfo {
  static constexpr ReferenceType reference_type = ReferenceType::NONE;
};

template <>
struct ReferenceInfo<adm::AudioProgramme, adm::AudioContent> {
  static constexpr ReferenceType reference_type = ReferenceType::MULTIPLE;
};

template <>
struct ReferenceInfo<adm::AudioContent, adm::AudioObject> {
  static constexpr ReferenceType reference_type = ReferenceType::MULTIPLE;
};

template <>
struct ReferenceInfo<adm::AudioObject, adm::AudioObject> {
  static constexpr ReferenceType reference_type = ReferenceType::MULTIPLE;
};

template <>
struct ReferenceInfo<adm::AudioObject, adm::AudioPackFormat> {
  static constexpr ReferenceType reference_type = ReferenceType::MULTIPLE;
};

template <>
struct ReferenceInfo<adm::AudioObject, adm::AudioTrackUid> {
  static constexpr ReferenceType reference_type = ReferenceType::MULTIPLE;
};

template <>
struct ReferenceInfo<adm::AudioPackFormat, adm::AudioPackFormat> {
  static constexpr ReferenceType reference_type = ReferenceType::MULTIPLE;
};

template <>
struct ReferenceInfo<adm::AudioPackFormat, adm::AudioChannelFormat> {
  static constexpr ReferenceType reference_type = ReferenceType::MULTIPLE;
};

template <>
struct ReferenceInfo<adm::AudioTrackUid, adm::AudioPackFormat> {
  static constexpr ReferenceType reference_type = ReferenceType::SINGLE;
};

template <>
struct ReferenceInfo<adm::AudioTrackUid, adm::AudioChannelFormat> {
  static constexpr ReferenceType reference_type = ReferenceType::SINGLE;
};

template <>
struct ReferenceInfo<adm::AudioTrackUid, adm::AudioTrackFormat> {
  static constexpr ReferenceType reference_type = ReferenceType::SINGLE;
};

template <>
struct ReferenceInfo<adm::AudioTrackFormat, adm::AudioStreamFormat> {
  static constexpr ReferenceType reference_type = ReferenceType::SINGLE;
};

template <>
struct ReferenceInfo<adm::AudioStreamFormat, adm::AudioTrackFormat> {
  static constexpr ReferenceType reference_type = ReferenceType::WEAK_TRACK;
};

template <>
struct ReferenceInfo<adm::AudioStreamFormat, adm::AudioChannelFormat> {
  static constexpr ReferenceType reference_type = ReferenceType::SINGLE;
};

}  // namespace detail

/// call f on each element of type To referenced by el
/// if there is no reference of this type, do nothing
template <typename To, typename From, typename F>
void for_each_reference_t(const std::shared_ptr<From> &el, F f) {
  using detail::ReferenceInfo;
  using detail::ReferenceType;

  constexpr ReferenceType reference_type = ReferenceInfo<From, To>::reference_type;

  if constexpr (reference_type == ReferenceType::MULTIPLE) {
    for (auto &subelement : el->template getReferences<To>()) f(subelement);
  } else if constexpr (reference_type == ReferenceType::SINGLE) {
    auto subelement = el->template getReference<To>();
    if (subelement) f(subelement);
  } else if constexpr (reference_type == ReferenceType::WEAK_TRACK) {
    for (auto &weak_track : el->getAudioTrackFormatReferences()) {
      auto track = weak_track.lock();
      if (track) f(track);
    }
  } else if constexpr (reference_type == ReferenceType::NONE) {
  }
}

/// get the number of references from el to To elements
///
/// the reference type must exist in libadm
template <typename To, typename From>
size_t count_references(const std::shared_ptr<From> &el) {
  using detail::ReferenceInfo;
  using detail::ReferenceType;

  constexpr ReferenceType reference_type = ReferenceInfo<std::remove_cvref_t<From>, To>::reference_type;

  static_assert(reference_type != ReferenceType::NONE, "reference type does not exist");

  if constexpr (reference_type == ReferenceType::MULTIPLE) {
    return el->template getReferences<To>().size();
  } else if constexpr (reference_type == ReferenceType::SINGLE) {
    return el->template getReference<To>() ? 1 : 0;
  } else if constexpr (reference_type == ReferenceType::WEAK_TRACK) {
    return el->getAudioTrackFormatReferences().size();
  } else if constexpr (reference_type == ReferenceType::NONE) {
    return 0;
  }
}

/// call f on each element referenced by el
template <typename Element, typename F>
void for_each_reference(const std::shared_ptr<Element> &el, F f) {
  for_each_reference_t<adm::AudioProgramme>(el, f);
  for_each_reference_t<adm::AudioContent>(el, f);
  for_each_reference_t<adm::AudioObject>(el, f);
  for_each_reference_t<adm::AudioPackFormat>(el, f);
  for_each_reference_t<adm::AudioChannelFormat>(el, f);
  for_each_reference_t<adm::AudioStreamFormat>(el, f);
  for_each_reference_t<adm::AudioTrackFormat>(el, f);
  for_each_reference_t<adm::AudioTrackUid>(el, f);
}

}  // namespace eat::utilities
