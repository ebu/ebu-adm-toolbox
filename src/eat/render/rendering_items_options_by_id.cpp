#include "eat/render/rendering_items_options_by_id.hpp"

#include <adm/document.hpp>

namespace eat::render {

SelectionOptionsId::SelectionOptionsId(SelectionStartId start_) : start(start_) {}

namespace {
struct StartFromIdsVisitor {
  const std::shared_ptr<adm::Document> &doc;

  SelectionStart operator()(const DefaultStart &ds) noexcept { return ds; }

  SelectionStart operator()(const ProgrammeIdStart &id) {
    auto prog = doc->lookup(id);
    if (!prog) throw std::runtime_error("programme with id " + adm::formatId(id) + " not found");
    return prog;
  }

  SelectionStart operator()(const ContentIdStart &ids) {
    ContentStart contents;
    for (auto &id : ids) {
      auto content = doc->lookup(id);
      if (!content) throw std::runtime_error("content with id " + adm::formatId(id) + " not found");
      contents.push_back(std::move(content));
    }
    return contents;
  }

  SelectionStart operator()(const ObjectIdStart &ids) {
    ObjectStart objects;
    for (auto &id : ids) {
      auto object = doc->lookup(id);
      if (!object) throw std::runtime_error("object with id " + adm::formatId(id) + " not found");
      objects.push_back(std::move(object));
    }
    return objects;
  }
};
}  // namespace

/// convert options with IDs to options with references
SelectionOptions selection_options_from_ids(const std::shared_ptr<adm::Document> &doc,
                                            const SelectionOptionsId &options) {
  return {std::visit(StartFromIdsVisitor{doc}, options.start)};
}

}  // namespace eat::render
