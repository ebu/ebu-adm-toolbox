#pragma once
#include <adm/document.hpp>
#include <any>
#include <functional>
#include <memory>

namespace eat::utilities::element_visitor {

class Visitable;
using VisitablePtr = std::shared_ptr<Visitable>;

using Path = std::vector<VisitablePtr>;

/// interface for values that are visitable using the visit functions below
class Visitable {
 public:
  virtual ~Visitable() {}

  /// visit the sub-elements described by desc
  /// returns true if desc is valid forthis type of value
  virtual bool visit(const std::string &desc, const std::function<void(VisitablePtr)> &) {
    (void)desc;
    return false;
  }

  /// get the held value as a std::any
  virtual std::any as_any() = 0;

  /// get the hald value
  template <typename T>
  auto as_t() {
    return std::any_cast<T>(as_any());
  }

  /// get a description for this element
  /// - for elements with an ID, returns the ID
  /// - for single elements (e.g. name), return the name of the element
  ///   (possibly shortened for elements whose name contains the parent element
  ///   name)
  /// - for repeated elements, returns the type and something which identifies
  ///   them (e.g. the value itself or a name)
  virtual std::string get_description() { return ""; };
};

/// visit sub-elements of start based on the path described by desc; calls cb
/// once for each element
void visit(const VisitablePtr &start, const std::vector<std::string> &desc,
           const std::function<void(const Path &path)> &cb);

/// visit sub-elements of document based on the path described by desc; calls cb
/// once for each element
void visit(std::shared_ptr<adm::Document> document, const std::vector<std::string> &desc,
           const std::function<void(const Path &path)> &cb);

/// visit sub-elements of document based on the path described by desc; calls cb
/// once for each element
///
/// WARNING: this uses const_cast to remove the const of adm::Document --
/// therefore the elements referenced in the path must be treated as if they were const
void visit(std::shared_ptr<const adm::Document> document, const std::vector<std::string> &desc,
           const std::function<void(const Path &path)> &cb);

/// turn a path into a list of strings using get_description
///
/// if the path has more than one element, the leading document element is
/// dropped as this is normally obvious from context
std::vector<std::string> path_to_strings(const Path &path);

/// format a path by joining the element with periods
std::string dotted_path(const std::vector<std::string> &desc);

/// format a path by concatenating the elements in reverse with " in "
/// e.g. {"APR_1001", "name"} -> "name in APR_1001"
std::string format_path(const std::vector<std::string> &path);

}  // namespace eat::utilities::element_visitor
