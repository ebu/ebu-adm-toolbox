#include "eat/utilities/element_visitor.hpp"

#include <adm/document.hpp>
#include <cassert>

#include "eat/utilities/unwrap_named.hpp"
#include "eat/utilities/unwrap_shared.hpp"

namespace eat::utilities::element_visitor {

using namespace adm;
using namespace eat::utilities;

namespace {

/// implementation of Visitable which just stores T
template <typename T>
class VisitableImpl : public Visitable {
 public:
  VisitableImpl(T value_) noexcept : value(std::move(value_)) {}

  virtual std::any as_any() override { return value; }

  // need to define functions which we want to specialise

  bool visit(const std::string &desc, const std::function<void(VisitablePtr)> &) override {
    (void)desc;
    return false;
  }

  std::string get_description() override { return ""; };

  /// get the value, removing named types and shared pointers
  auto &&ref() { return unwrap_named(unwrap_shared(value)); }

 protected:
  T value;
};

/// Visitable implementation which stores the description, for generic types which may be ysed in multiple places
template <typename T>
class VisitableImplWithDescription : public VisitableImpl<T> {
 public:
  VisitableImplWithDescription(T value_, std::string description_) noexcept
      : VisitableImpl<T>(std::move(value_)), description(std::move(description_)) {}

  std::string get_description() override { return description; };

 protected:
  std::string description;
};

// builders for the above two classes

template <typename T>
std::shared_ptr<Visitable> make_visitable(T &&value) {
  using PlainT = std::remove_cvref_t<T>;
  return std::make_shared<VisitableImpl<PlainT>>(std::forward<T>(value));
}

template <typename T>
std::shared_ptr<Visitable> make_visitable(T &&value, std::string description) {
  using PlainT = std::remove_cvref_t<T>;
  return std::make_shared<VisitableImplWithDescription<PlainT>>(std::forward<T>(value), std::move(description));
}

// macros for implementing Visitable::visit; these check the name, and if it
// matches call the callback and return true. the visit implementations are
// expected to use a sequence of these, then return false if all of them fell
// through

#define HANDLE_ELEMENTS(name, type)                                      \
  if (desc == #name) {                                                   \
    for (auto &ref : ref().getElements<type>()) cb(make_visitable(ref)); \
    return true;                                                         \
  }

#define HANDLE_REFERENCES(name, type)                                      \
  if (desc == #name) {                                                     \
    for (auto &ref : ref().getReferences<type>()) cb(make_visitable(ref)); \
    return true;                                                           \
  }

#define HANDLE_REFERENCE(name, type)                           \
  if (desc == #name) {                                         \
    auto referenced = ref().getReference<type>();              \
    if (referenced != nullptr) cb(make_visitable(referenced)); \
    return true;                                               \
  }

#define HANDLE_ATTRIBUTE(name, type)                                       \
  if (desc == #name) {                                                     \
    if (ref().template has<type>() && !ref().template isDefault<type>())   \
      cb(make_visitable(unwrap_named(ref().template get<type>()), #name)); \
    return true;                                                           \
  }

#define HANDLE_VECTOR_ATTRIBUTE(name, type)                                                         \
  if (desc == #name) {                                                                              \
    for (auto &sub_value : ref().template get<type>()) cb(make_visitable(unwrap_named(sub_value))); \
    return true;                                                                                    \
  }

#define HANDLE_VECTOR_ATTRIBUTE_NOUNWRAP(name, type)                                  \
  if (desc == #name) {                                                                \
    for (auto &sub_value : ref().template get<type>()) cb(make_visitable(sub_value)); \
    return true;                                                                      \
  }

// implementations of visit and get_description

template <>
bool VisitableImpl<std::shared_ptr<Document>>::visit(const std::string &desc,
                                                     const std::function<void(VisitablePtr)> &cb) {
  HANDLE_ELEMENTS(audioProgramme, AudioProgramme);
  HANDLE_ELEMENTS(audioContent, AudioContent);
  HANDLE_ELEMENTS(audioObject, AudioObject);
  HANDLE_ELEMENTS(audioPackFormat, AudioPackFormat);
  HANDLE_ELEMENTS(audioChannelFormat, AudioChannelFormat);
  HANDLE_ELEMENTS(audioStreamFormat, AudioStreamFormat);
  HANDLE_ELEMENTS(audioTrackFormat, AudioTrackFormat);
  HANDLE_ELEMENTS(audioTrackUid, AudioTrackUid);

  HANDLE_ATTRIBUTE(version, Version);
  return false;
}

template <>
std::string VisitableImpl<std::shared_ptr<Document>>::get_description() {
  return "document";
}

template <>
bool VisitableImpl<std::shared_ptr<AudioProgramme>>::visit(const std::string &desc,
                                                           const std::function<void(VisitablePtr)> &cb) {
  HANDLE_ATTRIBUTE(name, AudioProgrammeName);
  HANDLE_ATTRIBUTE(language, AudioProgrammeLanguage);
  HANDLE_VECTOR_ATTRIBUTE(label, Labels);

  HANDLE_REFERENCES(audioContent, AudioContent);
  return false;
}

template <>
bool VisitableImpl<std::shared_ptr<AudioContent>>::visit(const std::string &desc,
                                                         const std::function<void(VisitablePtr)> &cb) {
  HANDLE_ATTRIBUTE(name, AudioContentName);
  HANDLE_ATTRIBUTE(language, AudioContentLanguage);
  HANDLE_VECTOR_ATTRIBUTE(label, Labels);

  HANDLE_ATTRIBUTE(dialogue, DialogueId);

  HANDLE_REFERENCES(audioObject, AudioObject);
  return false;
}

template <>
bool VisitableImpl<std::shared_ptr<AudioObject>>::visit(const std::string &desc,
                                                        const std::function<void(VisitablePtr)> &cb) {
  HANDLE_ATTRIBUTE(name, AudioObjectName);
  HANDLE_VECTOR_ATTRIBUTE(label, Labels);
  HANDLE_VECTOR_ATTRIBUTE_NOUNWRAP(groupLabel, AudioComplementaryObjectGroupLabels);

  HANDLE_ATTRIBUTE(interact, Interact);
  HANDLE_ATTRIBUTE(start, Start);
  HANDLE_ATTRIBUTE(duration, Duration);
  HANDLE_ATTRIBUTE(dialogue, DialogueId);
  HANDLE_ATTRIBUTE(importance, Importance);
  HANDLE_ATTRIBUTE(disableDucking, DisableDucking);

  HANDLE_REFERENCES(audioObject, AudioObject);
  HANDLE_REFERENCES(audioPackFormat, AudioPackFormat);
  HANDLE_REFERENCES(audioTrackUid, AudioTrackUid);
  return false;
}

template <>
bool VisitableImpl<std::shared_ptr<AudioTrackUid>>::visit(const std::string &desc,
                                                          const std::function<void(VisitablePtr)> &cb) {
  HANDLE_REFERENCE(audioPackFormat, AudioPackFormat);
  HANDLE_REFERENCE(audioTrackFormat, AudioTrackFormat);
  HANDLE_REFERENCE(audioChannelFormat, AudioChannelFormat);
  return false;
}

template <>
bool VisitableImpl<std::shared_ptr<AudioPackFormat>>::visit(const std::string &desc,
                                                            const std::function<void(VisitablePtr)> &cb) {
  HANDLE_ATTRIBUTE(name, AudioPackFormatName);
  HANDLE_REFERENCES(audioPackFormat, AudioPackFormat);
  HANDLE_REFERENCES(audioChannelFormat, AudioChannelFormat);
  return false;
}

template <>
bool VisitableImpl<std::shared_ptr<AudioChannelFormat>>::visit(const std::string &desc,
                                                               const std::function<void(VisitablePtr)> &cb) {
  HANDLE_ATTRIBUTE(name, AudioChannelFormatName);

  if (desc == "audioBlockFormat[objects,polar]") {
    for (auto &bf : ref().getElements<AudioBlockFormatObjects>())
      if (bf.template has<SphericalPosition>()) cb(make_visitable(bf));
    return true;
  }
  if (desc == "audioBlockFormat[objects,cartesian]") {
    for (auto &bf : ref().getElements<AudioBlockFormatObjects>())
      if (bf.template has<CartesianPosition>()) cb(make_visitable(bf));
    return true;
  }
  return false;
}

template <>
bool VisitableImpl<AudioBlockFormatObjects>::visit(const std::string &desc,
                                                   const std::function<void(VisitablePtr)> &cb) {
  HANDLE_ATTRIBUTE(sphericalPosition, SphericalPosition);
  HANDLE_ATTRIBUTE(cartesianPosition, CartesianPosition);
  HANDLE_ATTRIBUTE(divergence, ObjectDivergence);
  return false;
}

template <>
bool VisitableImpl<SphericalPosition>::visit(const std::string &desc, const std::function<void(VisitablePtr)> &cb) {
  HANDLE_ATTRIBUTE(azimuth, Azimuth);
  HANDLE_ATTRIBUTE(elevation, Elevation);
  HANDLE_ATTRIBUTE(distance, Distance);
  return false;
}

template <>
bool VisitableImpl<CartesianPosition>::visit(const std::string &desc, const std::function<void(VisitablePtr)> &cb) {
  HANDLE_ATTRIBUTE(X, X);
  HANDLE_ATTRIBUTE(Y, Y);
  HANDLE_ATTRIBUTE(Z, Z);
  return false;
}

template <>
bool VisitableImpl<ObjectDivergence>::visit(const std::string &desc, const std::function<void(VisitablePtr)> &cb) {
  HANDLE_ATTRIBUTE(divergence, Divergence);
  HANDLE_ATTRIBUTE(azimuthRange, AzimuthRange);
  HANDLE_ATTRIBUTE(positionRange, PositionRange);
  return false;
}

template <>
std::string VisitableImpl<std::shared_ptr<AudioProgramme>>::get_description() {
  return formatId(value->template get<AudioProgrammeId>());
}

template <>
std::string VisitableImpl<std::shared_ptr<AudioContent>>::get_description() {
  return formatId(value->template get<AudioContentId>());
}

template <>
std::string VisitableImpl<std::shared_ptr<AudioObject>>::get_description() {
  return formatId(value->template get<AudioObjectId>());
}

template <>
std::string VisitableImpl<std::shared_ptr<AudioPackFormat>>::get_description() {
  return formatId(value->template get<AudioPackFormatId>());
}

template <>
std::string VisitableImpl<std::shared_ptr<AudioChannelFormat>>::get_description() {
  return formatId(value->template get<AudioChannelFormatId>());
}

template <>
std::string VisitableImpl<std::shared_ptr<AudioTrackUid>>::get_description() {
  return formatId(value->template get<AudioTrackUidId>());
}

template <>
std::string VisitableImpl<std::shared_ptr<AudioTrackFormat>>::get_description() {
  return formatId(value->template get<AudioTrackFormatId>());
}

template <>
std::string VisitableImpl<std::shared_ptr<AudioStreamFormat>>::get_description() {
  return formatId(value->template get<AudioStreamFormatId>());
}

template <>
std::string VisitableImpl<AudioBlockFormatObjects>::get_description() {
  return formatId(value.template get<AudioBlockFormatId>());
}

template <>
bool VisitableImpl<Label>::visit(const std::string &desc, const std::function<void(VisitablePtr)> &cb) {
  HANDLE_ATTRIBUTE(value, LabelValue);
  HANDLE_ATTRIBUTE(language, LabelLanguage);
  return false;
}

template <>
bool VisitableImpl<AudioComplementaryObjectGroupLabel>::visit(const std::string &desc,
                                                              const std::function<void(VisitablePtr)> &cb) {
  HANDLE_ATTRIBUTE(value, LabelValue);
  HANDLE_ATTRIBUTE(language, LabelLanguage);
  return false;
}

template <>
std::string VisitableImpl<Label>::get_description() {
  return "label \"" + value.template get<LabelValue>().get() + "\"";
}

template <>
std::string VisitableImpl<AudioComplementaryObjectGroupLabel>::get_description() {
  return "groupLabel \"" + value.get().template get<LabelValue>().get() + "\"";
}

/// recursive visit implementation
///
/// this:
/// - pushes el to the path
/// - recurses with the element described by desc[idx] and idx+1 by using
///   el->visit, or calls cb with path in the base case
/// - pops from the path to leave it as it was
void visit_impl(const VisitablePtr &el, const std::vector<std::string> &desc, size_t idx, Path &path,
                const std::function<void(const Path &path)> &cb) {
  path.push_back(el);

  if (idx < desc.size()) {
    bool res = el->visit(desc[idx], [&](const VisitablePtr &sub_el) { visit_impl(sub_el, desc, idx + 1, path, cb); });
    if (!res)
      throw std::runtime_error("path element '" + desc[idx] + "' is not visitable from " + el->get_description() +
                               " element");
  } else
    cb(path);

  path.pop_back();
}

}  // namespace

void visit(const VisitablePtr &start, const std::vector<std::string> &desc,
           const std::function<void(const Path &path)> &cb) {
  Path path;
  visit_impl(start, desc, 0, path, cb);
  assert(path.size() == 0);
}

void visit(std::shared_ptr<Document> document, const std::vector<std::string> &desc,
           const std::function<void(const Path &path)> &cb) {
  eat::utilities::element_visitor::visit(make_visitable(document), desc, cb);
}

void visit(std::shared_ptr<const Document> document, const std::vector<std::string> &desc,
           const std::function<void(const Path &path)> &cb) {
  eat::utilities::element_visitor::visit(std::const_pointer_cast<Document>(document), desc, cb);
}

std::vector<std::string> path_to_strings(const Path &path) {
  std::vector<std::string> strings;

  for (size_t i = 0; i < path.size(); i++) {
    std::string description = path[i]->get_description();
    if (i == 0 && path.size() > 1 && description == "document") continue;

    if (description.size()) strings.push_back(std::move(description));
  }
  return strings;
}

std::string dotted_path(const std::vector<std::string> &desc) {
  std::string result;

  for (const std::string &el : desc) {
    if (result.size()) result += ".";
    result += el;
  }

  return result;
}

std::string format_path(const std::vector<std::string> &path) {
  std::string result;

  for (auto it = path.rbegin(); it != path.rend(); it++) {
    if (result.size()) result += " in ";
    result += *it;
  }

  return result;
}

}  // namespace eat::utilities::element_visitor
