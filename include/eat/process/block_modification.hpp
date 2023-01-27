//
// Created by Richard Bailey on 25/10/2022.
//
#pragma once
#include <adm/elements/time.hpp>
#include <adm/elements_fwd.hpp>
#include <memory>
#include <optional>
#include <tuple>

namespace eat::process {
// Splits an input block into two, with the second starting at the input RTime
// split_point must be greater than the input rtime and less than input rtime + input duration
// First returned block has all interpolateable parameters linearly interpolated between
// their value in the prior block and those in the input block, based on the split point (values apply to the end of a
// block) Second returned block has parameters copied from input block This function currently assumes jumpposition is
// not set and interpolates on that basis.
[[nodiscard]] std::pair<adm::AudioBlockFormatObjects, adm::AudioBlockFormatObjects> split(
    std::optional<adm::AudioBlockFormatObjects> const &prior_block, adm::AudioBlockFormatObjects const &block_to_split,
    adm::Rtime const &split_point);

void clear_id(adm::AudioBlockFormatObjects &object);

std::vector<std::shared_ptr<adm::AudioChannelFormat>> referenced_channel_formats(adm::Document &doc);

std::vector<std::shared_ptr<adm::AudioChannelFormat>> only_object_type(
    std::vector<std::shared_ptr<adm::AudioChannelFormat>> const &input);

}  // namespace eat::process
