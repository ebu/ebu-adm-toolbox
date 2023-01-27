//
// Created by Richard Bailey on 08/09/2022.
//
#pragma once
#include <adm/elements/audio_channel_format.hpp>
#include <cassert>
#include <numeric>

#include "eat/framework/process.hpp"
#include "eat/process/adm_bw64.hpp"

namespace adm {
class AudioChannelFormat;
}

namespace eat {
namespace process {
std::vector<adm::AudioBlockFormatObjects> remove_jump_position(
    adm::BlockFormatsRange<adm::AudioBlockFormatObjects> input_blocks);

class JumpPositionRemover : public framework::FunctionalAtomicProcess {
 public:
  explicit JumpPositionRemover(std::string const &name);
  void process() override;

 private:
  framework::DataPortPtr<ADMData> in_axml;
  framework::DataPortPtr<ADMData> out_axml;
};

framework::ProcessPtr make_jump_position_remover(const std::string &name);
}  // namespace process
}  // namespace eat
