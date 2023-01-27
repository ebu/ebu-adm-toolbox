//
// Created by Richard Bailey on 08/09/2022.
//
#include "eat/process/jump_position_removal.hpp"

#include <adm/elements/audio_channel_format.hpp>
#include <adm/elements/audio_track_uid.hpp>
#include <cmath>
#include <numeric>

#include "eat/process/adm_time_extras.hpp"
#include "eat/process/block_modification.hpp"

namespace {

using namespace adm;
using namespace std::chrono_literals;
using eat::admx::minus;
using eat::admx::plus;
using eat::admx::roundToFractional;

bool interpolated_over_whole_block(adm::Duration const &duration, adm::InterpolationLength const &interpolationLength) {
  if (duration->isNanoseconds()) {
    return duration->asNanoseconds() - interpolationLength.get() == 0ns;
  } else {
    // Is this the correct way to check this?
    auto half_period = Time(FractionalTime(1, duration->asFractional().denominator() * 2)).asNanoseconds().count();
    return std::abs((duration->asNanoseconds() - interpolationLength.get()).count()) < half_period;
  }
}

template <typename BlockT>
bool should_be_split(BlockT const &block) {
  auto const jump = block.template get<JumpPosition>();
  auto const interpolation_length = jump.template get<InterpolationLength>();
  auto duration = block.template get<Duration>();
  return jump.template get<JumpPositionFlag>().get() && !interpolated_over_whole_block(duration, interpolation_length);
}

std::pair<Duration, Duration> split_duration(Duration original_duration, InterpolationLength interpolation_length) {
  if (original_duration->isNanoseconds()) {
    return {Duration{interpolation_length.get()},
            Duration{minus(original_duration.get(), Time{interpolation_length.get()})}};
  } else {
    auto firstDuration = roundToFractional(interpolation_length.get(), original_duration->asFractional().denominator());
    return {Duration{firstDuration}, Duration{minus(original_duration->asFractional(), firstDuration)}};
  }
}

std::pair<adm::AudioBlockFormatObjects, adm::AudioBlockFormatObjects> split(
    adm::AudioBlockFormatObjects const &original) {
  AudioBlockFormatObjects first_block{original};
  AudioBlockFormatObjects second_block{original};
  auto interpolationLength = original.get<JumpPosition>().get<InterpolationLength>();

  second_block.set(Rtime{plus(original.get<Rtime>().get(), Time{interpolationLength.get()})});

  auto [first_duration, second_duration] = split_duration(original.get<Duration>(), interpolationLength);
  first_block.set(std::move(first_duration));
  second_block.set(std::move(second_duration));

  return {std::move(first_block), std::move(second_block)};
}

std::vector<adm::AudioBlockFormatObjects> split_jump_position_blocks(
    adm::BlockFormatsRange<adm::AudioBlockFormatObjects> inputBlocks) {
  std::vector<adm::AudioBlockFormatObjects> outputBlocks;
  for (auto const &block : inputBlocks) {
    if (should_be_split(block)) {
      auto [first, second] = split(block);
      outputBlocks.push_back(std::move(first));
      outputBlocks.push_back(std::move(second));
    } else {
      outputBlocks.push_back(block);
    }
  }
  return outputBlocks;
}

}  // namespace

namespace eat::process {
std::vector<adm::AudioBlockFormatObjects> remove_jump_position(
    adm::BlockFormatsRange<adm::AudioBlockFormatObjects> input_blocks) {
  std::vector<adm::AudioBlockFormatObjects> blocks;

  if (input_blocks.size() > 1) {
    blocks = split_jump_position_blocks(input_blocks);
  } else {
    blocks = std::vector<adm::AudioBlockFormatObjects>(input_blocks.begin(), input_blocks.end());
  }

  for (auto &block : blocks) {
    block.unset<adm::JumpPosition>();
    clear_id(block);
  }

  return blocks;
}

JumpPositionRemover::JumpPositionRemover(std::string const &name)
    : framework::FunctionalAtomicProcess(name),
      in_axml(add_in_port<framework::DataPort<ADMData>>("in_axml")),
      out_axml(add_out_port<framework::DataPort<ADMData>>("out_axml")) {}

void JumpPositionRemover::process() {
  auto adm = std::move(in_axml->get_value());
  auto input_doc = adm.document.move_or_copy();
  for (auto const &cf : only_object_type(referenced_channel_formats(*input_doc))) {
    auto resampled = remove_jump_position(cf->getElements<adm::AudioBlockFormatObjects>());
    cf->clearAudioBlockFormats();
    for (auto const &bf : resampled) {
      cf->add(bf);
    }
  }
  adm.document = std::move(input_doc);
  out_axml->set_value(std::move(adm));
}

framework::ProcessPtr make_jump_position_remover(const std::string &name) {
  return std::make_shared<JumpPositionRemover>(name);
}

}  // namespace eat::process
