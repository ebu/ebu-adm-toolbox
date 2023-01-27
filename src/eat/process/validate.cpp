#include "eat/process/validate.hpp"

namespace ev = eat::utilities::element_visitor;

namespace eat::process::validation {

std::vector<NumElements::Message> NumElements::run(const ADMData &adm) const {
  std::vector<Message> messages;

  ev::visit(adm.document.read(), path, [&](const ev::Path &path_refs) {
    size_t n = 0;
    ev::visit(path_refs.back(), {element}, [&](const ev::Path &) noexcept { n++; });
    if (!range(n)) messages.push_back({ev::path_to_strings(path_refs), element, n, relationship});
  });

  return messages;
}

std::vector<StringLength::Message> StringLength::run(const ADMData &adm) const {
  std::vector<Message> messages;

  ev::visit(adm.document.read(), path, [&](const ev::Path &path_refs) {
    std::string s = path_refs.back()->as_t<std::string>();
    size_t n = s.size();
    if (!range(n)) messages.push_back({ev::path_to_strings(path_refs), n});
  });

  return messages;
}

std::vector<ValidLanguage::Message> ValidLanguage::run(const ADMData &adm) const {
  std::vector<Message> messages;

  ev::visit(adm.document.read(), path, [&](const ev::Path &path_refs) {
    std::string s = path_refs.back()->as_t<std::string>();
    LanguageCodeType real_type = parse_language_code(s);
    if ((real_type & type) == LanguageCodeType::NONE)
      messages.push_back({ev::path_to_strings(path_refs), std::move(s)});
  });

  return messages;
}

std::vector<ElementPresent::Message> ElementPresent::run(const ADMData &adm) const {
  std::vector<Message> messages;

  ev::visit(adm.document.read(), path, [&](const ev::Path &path_refs) {
    bool is_present = false;
    ev::visit(path_refs.back(), {element}, [&](const ev::Path &) noexcept { is_present = true; });
    if (is_present != present) messages.push_back({ev::path_to_strings(path_refs), element, is_present});
  });

  return messages;
}

std::vector<ObjectContentOrNested::Message> ObjectContentOrNested::run(const ADMData &adm) const {
  std::vector<Message> messages;
  auto elements = adm.document.read()->getElements<adm::AudioObject>();

  for (auto &&element : elements) {
    bool nested = element->getReferences<adm::AudioObject>();
    bool content = element->getReferences<adm::AudioPackFormat>() || element->getReferences<adm::AudioTrackUid>();

    if (nested) {
      if (content) messages.push_back({element->get<adm::AudioObjectId>(), true});
    } else {
      if (!content) messages.push_back({element->get<adm::AudioObjectId>(), false});
    }
  }

  return messages;
}

ValidationResults ProfileValidator::run(const ADMData &adm) const {
  ValidationResults results;

  for (const auto &check : checks) {
    auto messages = std::visit(
        [&adm](const auto &c) {
          std::vector<Message> check_messages;
          for (auto &message : c.run(adm)) {
            check_messages.push_back(detail::to_variant<Message>(message));
          }
          return check_messages;
        },
        check);
    results.push_back({check, std::move(messages)});
  }

  return results;
}

namespace {

template <typename T>
std::string to_str(const T &value) {
  return std::to_string(value);
}

template <>
std::string to_str<std::string>(const std::string &value) {
  return value;
}

template <typename T>
std::string format_options(const std::vector<T> &options) {
  std::string s;
  for (size_t i = 0; i < options.size(); i++) {
    if (options.size() > 1 && i == options.size() - 1)
      s += " or ";
    else if (i > 0)
      s += ", ";
    s += to_str(options.at(i));
  }
  return s;
}

std::string format_ev_dotted_path(const std::vector<std::string> &path) {
  if (path.size() == 0)
    return "document";
  else
    return ev::dotted_path(path) + " elements";
}
}  // namespace

// one visitor for both checks and messages so that we can keep related formats close together
struct FormatVisitor {
  std::string operator()(const NumElements &c) {
    return format_ev_dotted_path(c.path) + " must have " + c.range.format() + " " + c.element + " " + c.relationship;
  }

  std::string operator()(const NumElementsMessage &m) {
    return ev::format_path(m.path) + " has " + std::to_string(m.n) + " " + m.element + " " + m.relationship;
  }

  std::string operator()(const ObjectContentOrNested &) {
    return "audioObjects must have either audioObjectIdRef or audioPackFormatIdRef and audioTrackUidRef elements";
  }

  std::string operator()(const ObjectContentOrNestedMessage &m) {
    return adm::formatId(m.object_id) + " has " + (m.both ? "both" : "neither");
  }

  std::string operator()(const StringLength &c) {
    return ev::dotted_path(c.path) + " must be " + c.range.format() + " characters long";
  }

  std::string operator()(const StringLengthMessage &m) {
    return ev::format_path(m.path) + " is " + std::to_string(m.n) + " characters long";
  }

  std::string operator()(const ValidLanguage &c) {
    return ev::dotted_path(c.path) + " must be " + format_language_code_types(c.type);
  }

  std::string operator()(const ValidLanguageMessage &m) { return ev::format_path(m.path) + " is " + m.value; }

  std::string operator()(const ElementPresent &c) {
    return format_ev_dotted_path(c.path) + " " + (c.present ? "must" : "must not") + " have " + c.element +
           " attributes";
  }

  std::string operator()(const ElementPresentMessage &m) {
    return ev::format_path(m.path) + (m.present ? " has a " : " has no ") + m.element + " attribute";
  }

  template <typename T>
  std::string operator()(const UniqueElements<T> &c) {
    return format_ev_dotted_path(c.path1) + " must have unique " + ev::dotted_path(c.path2) + " attributes";
  }

  template <typename T>
  std::string operator()(const UniqueElementsMessage<T> &m) {
    return "in " + ev::format_path(m.path1) + ", " + ev::format_path(m.path2a) + " and " + ev::format_path(m.path2b) +
           " are both " + to_str(m.value);
  }

  template <typename T>
  std::string operator()(const ElementInRange<T> &c) {
    return ev::dotted_path(c.path) + " must be " + c.range.format();
  }

  template <typename T>
  std::string operator()(const ElementInRangeMessage<T> &m) {
    return ev::format_path(m.path) + " is " + to_str(m.value);
  }

  template <typename T>
  std::string operator()(const ElementInList<T> &c) {
    return ev::dotted_path(c.path) + " must be " + format_options(c.options);
  }

  template <typename T>
  std::string operator()(const ElementInListMessage<T> &m) {
    return ev::format_path(m.path) + " is " + to_str(m.value);
  }
};

std::string format_check(const Check &check) { return std::visit(FormatVisitor{}, check); }

std::string format_message(const Message &message) { return std::visit(FormatVisitor{}, message); }

void format_results(std::ostream &s, const ValidationResults &results, bool show_checks_without_messages) {
  for (auto &result : results) {
    if (show_checks_without_messages || result.messages.size()) {
      s << "check: " << format_check(result.check) << "\n";
      for (auto &message : result.messages) s << "    message: " << format_message(message) << "\n";
    }
  }
}

bool any_messages(const ValidationResults &results) {
  for (auto &result : results)
    if (result.messages.size()) return true;

  return false;
}

ProfileValidator make_emission_profile_validator(int level) {
  std::vector<Check> checks;
  if (level == 0) {
    checks.push_back(NumElements{{}, "audioProgramme", CountRange::at_least(1), "elements"});
    checks.push_back(NumElements{{}, "audioProgramme", CountRange::at_least(1), "elements"});
    checks.push_back(NumElements{{}, "audioContent", CountRange::at_least(1), "elements"});
    checks.push_back(NumElements{{}, "audioObject", CountRange::at_least(1), "elements"});
    checks.push_back(NumElements{{}, "audioTrackUid", CountRange::at_least(1), "elements"});

    checks.push_back(NumElements{{"audioProgramme"}, "audioContent", CountRange::at_least(1), "references"});
    checks.push_back(NumElements{{"audioObject"}, "audioTrackUid", CountRange::at_least(1), "references"});
  } else if (level == 1) {
    checks.push_back(NumElements{{}, "audioProgramme", CountRange::between(1, 8), "elements"});
    checks.push_back(NumElements{{}, "audioContent", CountRange::between(1, 16), "elements"});
    checks.push_back(NumElements{{}, "audioObject", CountRange::between(1, 48), "elements"});
    checks.push_back(NumElements{{}, "audioPackFormat", CountRange::between(0, 32), "elements"});
    checks.push_back(NumElements{{}, "audioChannelFormat", CountRange::between(0, 32), "elements"});
    checks.push_back(NumElements{{}, "audioTrackUid", CountRange::between(1, 32), "elements"});

    checks.push_back(NumElements{{"audioProgramme"}, "audioContent", CountRange::between(1, 16), "references"});
    checks.push_back(NumElements{{"audioObject"}, "audioObject", CountRange::up_to(16), "references"});
    checks.push_back(NumElements{{"audioObject"}, "audioTrackUid", CountRange::between(1, 12), "references"});

    checks.push_back(NumElements{{"audioProgramme"}, "label", CountRange::up_to(4), "elements"});
    checks.push_back(NumElements{{"audioContent"}, "label", CountRange::up_to(4), "elements"});
    checks.push_back(NumElements{{"audioObject"}, "label", CountRange::up_to(4), "elements"});
    checks.push_back(NumElements{{"audioObject"}, "groupLabel", CountRange::up_to(4), "elements"});
  } else if (level == 2) {
    checks.push_back(NumElements{{}, "audioProgramme", CountRange::between(1, 16), "elements"});
    checks.push_back(NumElements{{}, "audioContent", CountRange::between(1, 28), "elements"});
    checks.push_back(NumElements{{}, "audioObject", CountRange::between(1, 84), "elements"});
    checks.push_back(NumElements{{}, "audioPackFormat", CountRange::between(0, 56), "elements"});
    checks.push_back(NumElements{{}, "audioChannelFormat", CountRange::between(0, 56), "elements"});
    checks.push_back(NumElements{{}, "audioTrackUid", CountRange::between(1, 56), "elements"});

    checks.push_back(NumElements{{"audioProgramme"}, "audioContent", CountRange::between(1, 28), "references"});
    checks.push_back(NumElements{{"audioObject"}, "audioObject", CountRange::up_to(28), "references"});
    checks.push_back(NumElements{{"audioObject"}, "audioTrackUid", CountRange::between(1, 24), "references"});

    checks.push_back(NumElements{{"audioProgramme"}, "label", CountRange::up_to(8), "elements"});
    checks.push_back(NumElements{{"audioContent"}, "label", CountRange::up_to(8), "elements"});
    checks.push_back(NumElements{{"audioObject"}, "label", CountRange::up_to(8), "elements"});
    checks.push_back(NumElements{{"audioObject"}, "groupLabel", CountRange::up_to(8), "elements"});
  } else
    throw std::runtime_error("unknown profile level");

  checks.push_back(ElementPresent{{}, "version", true});
  checks.push_back(ElementInList<std::string>{{"version"}, {"ITU-R_BS.2076-3"}});

  checks.push_back(NumElements{{"audioContent"}, "audioObject", CountRange::exactly(1), "references"});
  checks.push_back(NumElements{{"audioObject"}, "audioPackFormat", CountRange::between(0, 1), "references"});
  checks.push_back(NumElements{{"audioTrackUid"}, "audioPackFormat", CountRange::exactly(1), "references"});
  checks.push_back(NumElements{{"audioTrackUid"}, "audioChannelFormat", CountRange::exactly(1), "references"});
  checks.push_back(NumElements{{"audioTrackUid"}, "audioTrackFormat", CountRange::exactly(0), "references"});

  checks.push_back(ObjectContentOrNested{});

  checks.push_back(StringLength{{"audioProgramme", "name"}, CountRange::between(1, 64)});
  checks.push_back(StringLength{{"audioContent", "name"}, CountRange::between(1, 64)});
  checks.push_back(StringLength{{"audioObject", "name"}, CountRange::between(1, 64)});
  checks.push_back(StringLength{{"audioPackFormat", "name"}, CountRange::between(1, 64)});
  checks.push_back(StringLength{{"audioChannelFormat", "name"}, CountRange::between(1, 64)});

  checks.push_back(StringLength{{"audioProgramme", "label", "value"}, CountRange::between(1, 64)});
  checks.push_back(StringLength{{"audioContent", "label", "value"}, CountRange::between(1, 64)});
  checks.push_back(StringLength{{"audioObject", "label", "value"}, CountRange::between(1, 64)});
  checks.push_back(StringLength{{"audioObject", "groupLabel", "value"}, CountRange::between(1, 64)});

  checks.push_back(ValidLanguage{{"audioProgramme", "label", "language"}, LanguageCodeType::REGULAR});
  checks.push_back(ValidLanguage{{"audioContent", "label", "language"}, LanguageCodeType::REGULAR});
  checks.push_back(ValidLanguage{{"audioObject", "label", "language"}, LanguageCodeType::REGULAR});
  checks.push_back(ValidLanguage{{"audioObject", "groupLabel", "language"}, LanguageCodeType::REGULAR});

  checks.push_back(
      ValidLanguage{{"audioProgramme", "language"}, LanguageCodeType::REGULAR | LanguageCodeType::UNDETERMINED});
  checks.push_back(
      ValidLanguage{{"audioContent", "language"}, LanguageCodeType::REGULAR | LanguageCodeType::UNDETERMINED});

  checks.push_back(ElementPresent{{"audioProgramme", "label"}, "language", true});
  checks.push_back(ElementPresent{{"audioContent", "label"}, "language", true});
  checks.push_back(ElementPresent{{"audioObject", "label"}, "language", true});
  checks.push_back(ElementPresent{{"audioObject", "groupLabel"}, "language", true});

  checks.push_back(UniqueElements<std::string>{{"audioProgramme"}, {"label", "language"}});
  checks.push_back(UniqueElements<std::string>{{"audioContent"}, {"label", "language"}});
  checks.push_back(UniqueElements<std::string>{{"audioObject"}, {"label", "language"}});
  checks.push_back(UniqueElements<std::string>{{"audioObject"}, {"groupLabel", "language"}});

  // section 2.1.4 (audioContent)
  // table 8: dialogue present
  checks.push_back(ElementPresent{{"audioContent"}, "dialogue", true});

  // section 2.1.5 (audioObject)
  // table 9: interact present
  checks.push_back(ElementPresent{{"audioObject"}, "interact", true});
  // table 9: all other attributes should not be present
  checks.push_back(ElementPresent{{"audioObject"}, "start", false});
  checks.push_back(ElementPresent{{"audioObject"}, "duration", false});
  checks.push_back(ElementPresent{{"audioObject"}, "dialogue", false});
  checks.push_back(ElementPresent{{"audioObject"}, "importance", false});
  checks.push_back(ElementPresent{{"audioObject"}, "disableDucking", false});

  // section 2.1.9
  // table 24
  // "Must include coordinate attributes for all three axes."
  // libadm ensures that polar positions have azimuth and elevation
  checks.push_back(
      ElementPresent{{"audioChannelFormat", "audioBlockFormat[objects,polar]", "sphericalPosition"}, "distance", true});
  // libadm ensures that cartesian positions have X and Y
  checks.push_back(
      ElementPresent{{"audioChannelFormat", "audioBlockFormat[objects,cartesian]", "cartesianPosition"}, "Z", true});

  // libadm checks the range of azimuth and elevation
  // "Distance shall be set to a value between 0.0 and 1.0"
  checks.push_back(
      ElementInRange<float>{{"audioChannelFormat", "audioBlockFormat[objects,polar]", "sphericalPosition", "distance"},
                            Range<float>::between(0.0f, 1.0f)});

  // "X, Y and Z shall be set to values between -1.0 and 1.0"
  checks.push_back(
      ElementInRange<float>{{"audioChannelFormat", "audioBlockFormat[objects,cartesian]", "cartesianPosition", "X"},
                            Range<float>::between(-1.0f, 1.0f)});
  checks.push_back(
      ElementInRange<float>{{"audioChannelFormat", "audioBlockFormat[objects,cartesian]", "cartesianPosition", "Y"},
                            Range<float>::between(-1.0f, 1.0f)});
  checks.push_back(
      ElementInRange<float>{{"audioChannelFormat", "audioBlockFormat[objects,cartesian]", "cartesianPosition", "Z"},
                            Range<float>::between(-1.0f, 1.0f)});

  // objectDivergence
  // range checks are done by libadm
  // "Must contain either azimuthRange or positionRange attribute depending upon coordinate system used"
  checks.push_back(ElementPresent{
      {"audioChannelFormat", "audioBlockFormat[objects,cartesian]", "divergence"}, "azimuthRange", false});
  checks.push_back(ElementPresent{
      {"audioChannelFormat", "audioBlockFormat[objects,cartesian]", "divergence"}, "positionRange", true});
  checks.push_back(
      ElementPresent{{"audioChannelFormat", "audioBlockFormat[objects,polar]", "divergence"}, "positionRange", false});
  checks.push_back(
      ElementPresent{{"audioChannelFormat", "audioBlockFormat[objects,polar]", "divergence"}, "azimuthRange", true});

  return {checks};
}

namespace {
struct MakeValidatorVisitor {
  ProfileValidator operator()(const profiles::ITUEmissionProfile &p) {
    return make_emission_profile_validator(p.level());
  }
};
}  // namespace

ProfileValidator make_profile_validator(const profiles::Profile &p) { return std::visit(MakeValidatorVisitor{}, p); }
}  // namespace eat::process::validation
