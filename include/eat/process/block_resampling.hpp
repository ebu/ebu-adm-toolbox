//
// Created by Richard Bailey on 27/09/2022.
//
#pragma once
#include <adm/elements/audio_block_format_objects.hpp>
#include <adm/elements/audio_channel_format.hpp>
#include <vector>

#include "eat/framework/process.hpp"
#include "eat/process/adm_bw64.hpp"

namespace eat::process {
/*
 * @Returns A vector of contiguous AudioBlockFormatObjects that, other than the first block, have a duration at least
 * as long as interval If the first block of the input has a zero length, this will be passed through as the first block
 * of the output
 *
 * Preconditions:
 *   All blocks in the input must have valid rtime and duration (i.e. are contiguous, and not defaulted to object
 * length)
 *
 * Note that the returned blocks will all have a default intialiased AudioBlockFormatID - this is so that they will have
 * correct ids assigned when added to an AudioChannelFormat.
 */
[[nodiscard]] std::vector<adm::AudioBlockFormatObjects> resample_to_minimum_preserving_zero(
    adm::BlockFormatsRange<adm::AudioBlockFormatObjects> blocks, adm::Time const &interval);
[[nodiscard]] std::vector<adm::AudioBlockFormatObjects> de_duplicate_zero_length_blocks(
    adm::BlockFormatsRange<adm::AudioBlockFormatObjects> blocks);

class BlockResampler : public framework::FunctionalAtomicProcess {
 public:
  explicit BlockResampler(std::string const &name, adm::Time min_duration);
  void process() override;

 private:
  framework::DataPortPtr<ADMData> in_axml;
  framework::DataPortPtr<ADMData> out_axml;
  adm::Time min_duration;
};

framework::ProcessPtr make_block_resampler(const std::string &name, std::string const &min_duration);

}  // namespace eat::process
