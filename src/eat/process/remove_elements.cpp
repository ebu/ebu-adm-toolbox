#include "eat/process/remove_elements.hpp"

#include <adm/document.hpp>
#include <adm/element_variant.hpp>
#include <vector>

#include "eat/framework/process.hpp"
#include "eat/process/adm_bw64.hpp"

using namespace eat::framework;

namespace eat::process {

struct RemoveElementVisitor : public boost::static_visitor<> {
  RemoveElementVisitor(std::shared_ptr<adm::Document> doc_) : doc(std::move(doc_)) {}

  template <typename T>
  void operator()(const T &id) {
    auto element = doc->lookup(id);
    if (!element) throw std::runtime_error("could not find " + adm::formatId(id));
    doc->remove(element);
  }

 private:
  std::shared_ptr<adm::Document> doc;
};

/// remove elements from a document
class RemoveElements : public FunctionalAtomicProcess {
 public:
  RemoveElements(const std::string &name, ElementIds ids_)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        out_axml(add_out_port<DataPort<ADMData>>("out_axml")),
        ids(std::move(ids_)) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());
    auto doc = adm.document.move_or_copy();

    RemoveElementVisitor visitor(doc);
    for (const auto &id : ids) boost::apply_visitor(visitor, id);

    adm.document = std::move(doc);

    out_axml->set_value(std::move(adm));
  }

 private:
  DataPortPtr<ADMData> in_axml;
  DataPortPtr<ADMData> out_axml;
  ElementIds ids;
};

framework::ProcessPtr make_remove_elements(const std::string &name, ElementIds ids) {
  return std::make_shared<RemoveElements>(name, std::move(ids));
}
}  // namespace eat::process
