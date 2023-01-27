#pragma once

#include <string>
#include <vector>

#include "eat/process/adm_bw64.hpp"

namespace eat::process {

class BlockSubElementDropper : public framework::FunctionalAtomicProcess {
 public:
  enum class Droppable {
    Diffuse,
    ChannelLock,
    ObjectDivergence,
    JumpPosition,
    ScreenRef,
    Width,
    Depth,
    Height,
    Gain,
    Importance,
    Headlocked,
    HeadphoneVirtualise
  };

  BlockSubElementDropper(std::string const &name, std::vector<Droppable> params_to_drop);
  void process() override;

 private:
  framework::DataPortPtr<ADMData> in_axml;
  framework::DataPortPtr<ADMData> out_axml;
  std::vector<Droppable> to_drop;
};

std::vector<BlockSubElementDropper::Droppable> parse_droppable(std::vector<std::string> const &to_drop);

framework::ProcessPtr make_block_subelement_dropper(std::string const &name,
                                                    std::vector<BlockSubElementDropper::Droppable> to_drop);
}  // namespace eat::process
