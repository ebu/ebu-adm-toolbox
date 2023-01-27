#include "eat/process/block_subelement_dropper.hpp"

#include <adm/adm.hpp>
#include <unordered_map>

#include "eat/process/block_modification.hpp"
using namespace eat::process;

namespace {
using enum BlockSubElementDropper::Droppable;
template <typename ParamT>
void unset_if_present(adm::AudioBlockFormatObjects &abf) {
  if (abf.has<ParamT>()) abf.unset<ParamT>();
}

void remove_parameter(adm::AudioBlockFormatObjects &abf, BlockSubElementDropper::Droppable param) {
  switch (param) {
    case Diffuse: {
      unset_if_present<adm::Diffuse>(abf);
      break;
    }
    case ChannelLock: {
      unset_if_present<adm::ChannelLock>(abf);
      break;
    }
    case ObjectDivergence: {
      unset_if_present<adm::ObjectDivergence>(abf);
      break;
    }
    case JumpPosition: {
      unset_if_present<adm::JumpPosition>(abf);
      break;
    }
    case ScreenRef: {
      unset_if_present<adm::ScreenRef>(abf);
      break;
    }
    case Width: {
      unset_if_present<adm::Width>(abf);
      break;
    }
    case Depth: {
      unset_if_present<adm::Depth>(abf);
      break;
    }
    case Height: {
      unset_if_present<adm::Height>(abf);
      break;
    }
    case Gain: {
      unset_if_present<adm::Gain>(abf);
      break;
    }
    case Importance: {
      unset_if_present<adm::Importance>(abf);
      break;
    }
    case Headlocked: {
      unset_if_present<adm::HeadLocked>(abf);
      break;
    }
    case HeadphoneVirtualise: {
      unset_if_present<adm::HeadphoneVirtualise>(abf);
      break;
    }
    default: {
      throw std::runtime_error("Unhandled droppable parameter type");
    }
  }
}
}  // namespace

namespace eat::process {
BlockSubElementDropper::BlockSubElementDropper(std::string const &name, std::vector<Droppable> params_to_drop)
    : framework::FunctionalAtomicProcess(name),
      in_axml(add_in_port<framework::DataPort<ADMData>>("in_axml")),
      out_axml(add_out_port<framework::DataPort<ADMData>>("out_axml")),
      to_drop(std::move(params_to_drop)) {}
void BlockSubElementDropper::process() {
  auto adm = std::move(in_axml->get_value());
  auto input_doc = adm.document.move_or_copy();
  for (auto const &cf : only_object_type(referenced_channel_formats(*input_doc))) {
    auto blocks = cf->getElements<adm::AudioBlockFormatObjects>();
    for (auto &block : blocks) {
      for (auto parameter : to_drop) {
        remove_parameter(block, parameter);
      }
    }
  }
  adm.document = std::move(input_doc);
  out_axml->set_value(std::move(adm));
}

eat::framework::ProcessPtr make_block_subelement_dropper(const std::string &name,
                                                         std::vector<BlockSubElementDropper::Droppable> to_drop) {
  return std::make_shared<BlockSubElementDropper>(name, std::move(to_drop));
}

std::vector<BlockSubElementDropper::Droppable> parse_droppable(const std::vector<std::string> &to_drop) {
  static std::unordered_map<std::string, BlockSubElementDropper::Droppable> const lookup{
      {"Diffuse", Diffuse},
      {"ChannelLock", ChannelLock},
      {"ObjectDivergence", ObjectDivergence},
      {"JumpPosition", JumpPosition},
      {"ScreenRef", ScreenRef},
      {"Width", Width},
      {"Depth", Depth},
      {"Height", Height},
      {"Gain", Gain},
      {"Importance", Importance},
      {"Headlocked", Headlocked},
      {"HeadphoneVirtualise", HeadphoneVirtualise}};

  std::vector<BlockSubElementDropper::Droppable> droppable;
  droppable.reserve(to_drop.size());
  for (auto const &str : to_drop) {
    auto it = lookup.find(str);
    if (it == lookup.end()) {
      throw(std::runtime_error(str + " is not a supported droppable sub-element"));
    }
    droppable.push_back(it->second);
  }
  return droppable;
}

}  // namespace eat::process
