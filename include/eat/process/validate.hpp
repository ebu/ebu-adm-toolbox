#pragma once
#include <adm/document.hpp>
#include <iosfwd>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "adm_bw64.hpp"
#include "eat/process/language_codes.hpp"
#include "eat/process/profiles.hpp"
#include "eat/process/validate_detail.hpp"
#include "eat/utilities/element_visitor.hpp"

/// \rst
/// .. cpp:namespace:: eat::process::validation
///
/// This implements validation for ADM documents.
///
/// To use this as a process, see :func:`eat::process::make_validate`.
///
/// To use it directly, see :func:`make_profile_validator` and
/// :func:`make_emission_profile_validator` to make a validator,
/// :func:`ProfileValidator::run` to run it, and :func:`any_messages` and
/// :func:`format_results` to interpret the results.
///
/// In general, validation checks are implemented as a struct whose members
/// configure the check, and which has a run function which performs the check
///
/// Run yields a vector of message objects, and the type of these corresponds
/// with the type of the check. For example, the :class:`NumElements` check
/// results in :class:`NumElementsMessage` messages.
///
/// These are combined together with variants, so a :type:`Check` is one of any
/// known checks, and a :type:`Message` is one of any known message.
///
/// Both checks and message types have additional functions to enable
/// meta-programming:
///
/// - A static name method gives the name of the check or message (the same as
///   the class name)
/// - A visit function accepts a callback and calls it once for each member
///   with the name of the member and a reference to the member.
///
/// These can be used to serialise and de-serialise checks and messages to
/// JSON, for example.
/// \endrst
namespace eat::process::validation {

/// a range check for numbers
template <typename T>
struct Range {
  std::optional<T> lower_limit;
  std::optional<T> upper_limit;

  bool operator()(T n) const { return (!lower_limit || n >= *lower_limit) && (!upper_limit || n <= *upper_limit); }

  std::string format() const {
    if (lower_limit && !upper_limit)
      return "at least " + std::to_string(*lower_limit);
    else if (!lower_limit && upper_limit)
      return "up to " + std::to_string(*upper_limit);
    else if (lower_limit && upper_limit && (*lower_limit == *upper_limit))
      return std::to_string(*lower_limit);
    else if (lower_limit && upper_limit)
      return "between " + std::to_string(*lower_limit) + " and " + std::to_string(*upper_limit);
    else
      throw std::runtime_error("formatting an open range");
  }

  static Range up_to(T upper_limit) { return Range{{}, upper_limit}; }

  static Range at_least(T lower_limit) { return Range{lower_limit, {}}; }

  static Range between(T lower_limit, T upper_limit) { return Range{lower_limit, upper_limit}; }

  static Range exactly(T limit) { return Range{limit, limit}; }
};

using CountRange = Range<size_t>;

struct NumElementsMessage {
  static std::string name() { return "NumElementsMessage"; }

  std::vector<std::string> path;
  std::string element;
  size_t n;
  std::string relationship = "elements";

  template <typename F>
  void visit(F f) {
    f("path", path);
    f("element", element);
    f("n", n);
    f("relationship", relationship);
  }
};

/// generic check for the number of elements within a containing element
///
/// this can check top-level elements, references, or real sub-elements
struct NumElements {
  static std::string name() { return "NumElements"; }

  /// path to the containing element (empty to check top-level elements
  std::vector<std::string> path;
  /// name of the element to count
  std::string element;
  /// acceptable number of elements
  CountRange range;
  /// plural relationship type only used for formatting the check/message
  std::string relationship = "elements";

  using Message = NumElementsMessage;

  std::vector<Message> run(const ADMData &adm) const;

  template <typename F>
  void visit(F f) {
    f("path", path);
    f("element", element);
    f("range", range);
    f("relationship", relationship);
  }
};

struct StringLengthMessage {
  static std::string name() { return "StringLengthMessage"; }

  std::vector<std::string> path;
  size_t n;

  template <typename F>
  void visit(F f) {
    f("path", path);
    f("n", n);
  }
};

/// check the length of a string
struct StringLength {
  static std::string name() { return "StringLength"; }

  /// path to a string
  std::vector<std::string> path;
  /// acceptable number of characters
  CountRange range;

  using Message = StringLengthMessage;

  std::vector<Message> run(const ADMData &adm) const;

  template <typename F>
  void visit(F f) {
    f("path", path);
    f("range", range);
  }
};

struct ValidLanguageMessage {
  static std::string name() { return "ValidLanguageMessage"; }

  std::vector<std::string> path;
  std::string value;

  template <typename F>
  void visit(F f) {
    f("path", path);
    f("value", value);
  }
};

/// check that an element contains a valid language code
struct ValidLanguage {
  static std::string name() { return "ValidLanguage"; }

  /// path to a string containing the language code
  std::vector<std::string> path;
  /// acceptable language code types
  LanguageCodeType type;

  using Message = ValidLanguageMessage;

  std::vector<Message> run(const ADMData &adm) const;

  template <typename F>
  void visit(F f) {
    f("path", path);
    f("type", type);
  }
};

struct ElementPresentMessage {
  static std::string name() { return "ElementPresentMessage"; }

  std::vector<std::string> path;
  std::string element;
  bool present;

  template <typename F>
  void visit(F f) {
    f("path", path);
    f("element", element);
    f("present", present);
  }
};

/// check if an element is present
///
/// this is really the same as NumElements, but with different formatting
struct ElementPresent {
  static std::string name() { return "ElementPresent"; }

  std::vector<std::string> path;
  std::string element;
  bool present;

  using Message = ElementPresentMessage;

  std::vector<Message> run(const ADMData &adm) const;

  template <typename F>
  void visit(F f) {
    f("path", path);
    f("element", element);
    f("present", present);
  }
};

template <typename T>
struct UniqueElementsMessage {
  static std::string name() { return "UniqueElementsMessage<" + detail::type_name<T> + ">"; }

  std::vector<std::string> path1;
  T value;
  std::vector<std::string> path2a;
  std::vector<std::string> path2b;

  template <typename F>
  void visit(F f) {
    f("path1", path1);
    f("value", value);
    f("path2a", path2a);
    f("path2b", path2b);
  }
};

/// check elements for uniqueness
///
/// for each element visited by path1, the elements from that visited by path2 must be unique
///
/// T is the type of the unwrapped element to check, e.g. string
template <typename T>
struct UniqueElements {
  static std::string name() { return "UniqueElements<" + detail::type_name<T> + ">"; }

  using Message = UniqueElementsMessage<T>;

  /// path to the element which contains the unique values
  std::vector<std::string> path1;
  /// path from the elements visited by path1 to the elements to check
  std::vector<std::string> path2;

  std::vector<Message> run(const ADMData &adm) const {
    std::vector<Message> messages;

    namespace ev = eat::utilities::element_visitor;

    // paths include the starting element, and this needs to be removed from the second path part
    auto remove_first = [](ev::Path path) {
      path.erase(path.begin());
      return path;
    };

    ev::visit(adm.document.read(), path1, [&](const ev::Path &path1_refs) {
      std::map<T, ev::Path> seen;
      ev::visit(path1_refs.back(), path2, [&](const ev::Path &path2_refs) {
        T value = path2_refs.back()->as_t<T>();
        if (auto it = seen.find(value); it != seen.end())
          messages.push_back({ev::path_to_strings(path1_refs), value, ev::path_to_strings(it->second),
                              ev::path_to_strings(remove_first(path2_refs))});
        else
          seen.emplace(value, remove_first(path2_refs));
      });
    });

    return messages;
  }

  template <typename F>
  void visit(F f) {
    f("path1", path1);
    f("path2", path2);
  }
};

template <typename T>
struct ElementInRangeMessage {
  static std::string name() { return "ElementInRangeMessage<" + detail::type_name<T> + ">"; }

  std::vector<std::string> path;
  T value;

  template <typename F>
  void visit(F f) {
    f("path", path);
    f("value", value);
  }
};

/// check that an element is in the given range
///
/// T is the unwrapped element type (e.g. float or int)
template <typename T>
struct ElementInRange {
  static std::string name() { return "ElementInRange<" + detail::type_name<T> + ">"; }

  using Message = ElementInRangeMessage<T>;

  std::vector<std::string> path;
  Range<T> range;

  std::vector<Message> run(const ADMData &adm) const {
    std::vector<Message> messages;

    namespace ev = eat::utilities::element_visitor;
    ev::visit(adm.document.read(), path, [&](const ev::Path &path_refs) {
      T value = path_refs.back()->as_t<T>();
      if (!range(value)) messages.push_back({ev::path_to_strings(path_refs), value});
    });

    return messages;
  }

  template <typename F>
  void visit(F f) {
    f("path", path);
    f("range", range);
  }
};

template <typename T>
struct ElementInListMessage {
  static std::string name() { return "ElementInListMessage<" + detail::type_name<T> + ">"; }

  std::vector<std::string> path;
  T value;

  template <typename F>
  void visit(F f) {
    f("path", path);
    f("value", value);
  }
};

/// check that an element is one of a given list of values
///
/// T is the unwrapped element type, e.g. string
template <typename T>
struct ElementInList {
  static std::string name() { return "ElementInList<" + detail::type_name<T> + ">"; }

  using Message = ElementInListMessage<T>;

  /// path to the elements to check
  std::vector<std::string> path;
  /// acceptable options
  std::vector<T> options;

  std::vector<Message> run(const ADMData &adm) const {
    std::vector<Message> messages;

    namespace ev = eat::utilities::element_visitor;

    ev::visit(adm.document.read(), path, [&](const ev::Path &path_refs) {
      T value = path_refs.back()->as_t<T>();

      bool found = false;
      for (auto &option : options)
        if (value == option) found = true;

      if (!found) messages.push_back({ev::path_to_strings(path_refs), value});
    });

    return messages;
  }

  template <typename F>
  void visit(F f) {
    f("path", path);
    f("options", options);
  }
};

struct ObjectContentOrNestedMessage {
  static std::string name() { return "ObjectContentOrNestedMessage"; }

  adm::AudioObjectId object_id;
  bool both;

  template <typename F>
  void visit(F f) {
    f("object_id", object_id);
    f("both", both);
  }
};

/// check that audioObjects have either audio content (pack or trackUid refs)
/// or nested audioObjects, not both or neither
struct ObjectContentOrNested {
  static std::string name() { return "ObjectContentOrNested"; }

  using Message = ObjectContentOrNestedMessage;

  std::vector<Message> run(const ADMData &adm) const;

  template <typename F>
  void visit(F) {}
};

/// known checks
using Check = std::variant<ElementInList<std::string>, ElementInRange<float>, ElementPresent, NumElements,
                           ObjectContentOrNested, StringLength, UniqueElements<std::string>, ValidLanguage>;

/// messages that known checks can produce
using Message = detail::ToMessages<Check>;

/// result of running a single check; holds the check, and the messages that it
/// resulted in
struct ValidationResult {
  Check check;
  std::vector<Message> messages;
};

/// results of all checks that have been ran
using ValidationResults = std::vector<ValidationResult>;

/// holds a list of checks which can be ran on some ADM data to yield some
/// results
class ProfileValidator {
 public:
  ProfileValidator(std::vector<Check> checks_) : checks(std::move(checks_)) {}

  /// run the checks on some ADM data
  ValidationResults run(const ADMData &adm) const;

 private:
  std::vector<Check> checks;
};

/// are there any error messages in results?
bool any_messages(const ValidationResults &results);

/// get an english representation for a single check
std::string format_check(const Check &check);
/// get an english representation for a single message
std::string format_message(const Message &message);
/// format results to a stream
///
/// either prints all checks and results, or only those with any messages,
/// depending on show_checks_without_messages
void format_results(std::ostream &s, const ValidationResults &results, bool show_checks_without_messages = false);

/// build a validator for a given emission profile level
ProfileValidator make_emission_profile_validator(int level);
/// build a validator for a known profile
ProfileValidator make_profile_validator(const profiles::Profile &);

}  // namespace eat::process::validation
