//
// Created by Richard Bailey on 25/10/2022.
//

#include "eat/process/block_modification.hpp"

#include <adm/document.hpp>
#include <adm/elements.hpp>

#include "eat/process/adm_time_extras.hpp"
namespace {
// not sure if this will work everywhere as c++20, but trivial to implement if not
template <typename Param, typename Parent>
Param lerp(Parent const &first, Parent const &second, float t) {
  return Param(std::lerp(static_cast<float>(first.template get<Param>().get()),
                         static_cast<float>(second.template get<Param>().get()), t));
}

adm::SphericalPosition lerped_position(adm::SphericalPosition const &prior, adm::SphericalPosition const &next,
                                       float t) {
  adm::SphericalPosition lerped{prior};  // copy over ScreenEdgeLock etc
  lerped.set(lerp<adm::Azimuth>(prior, next, t));
  lerped.set(lerp<adm::Elevation>(prior, next, t));
  lerped.set(lerp<adm::Distance>(prior, next, t));
  return lerped;
}

adm::CartesianPosition lerped_position(adm::CartesianPosition const &prior, adm::CartesianPosition const &next,
                                       float t) {
  adm::CartesianPosition lerped{prior};  // copy over ScreenEdgeLock etc
  lerped.set(lerp<adm::X>(prior, next, t));
  lerped.set(lerp<adm::Y>(prior, next, t));
  lerped.set(lerp<adm::Z>(prior, next, t));
  return lerped;
}

template <typename ParamT, typename ParentT>
bool either_not_default(ParentT const &first, ParentT const &second) {
  return !(first.template isDefault<ParamT>() && second.template isDefault<ParamT>());
}

template <typename ParamT, typename ParentT>
bool both_present(ParentT const &first, ParentT const &second) {
  return first.template has<ParamT>() && second.template has<ParamT>();
}

template <typename ParamT, typename ParentT>
bool both_present_either_not_default(ParentT const &first, ParentT second) {
  return both_present<ParamT>(first, second) && either_not_default<ParamT>(first, second);
};

template <typename ParamT, typename ParentT>
std::optional<ParamT> lerp_if_required(ParentT const &first, ParentT const &second, float t) {
  if (both_present_either_not_default<ParamT>(first, second)) {
    return lerp<ParamT>(first, second, t);
  }
  return {};
}

adm::AudioBlockFormatObjects lerped_block(std::optional<adm::AudioBlockFormatObjects> const &prior,
                                          adm::AudioBlockFormatObjects const &next, float t) {
  // if there is no preceding block, nothing to interpolate from
  if (!prior) {
    return next;
  }

  auto prior_spherical = prior->has<adm::SphericalPosition>();
  auto next_spherical = next.has<adm::SphericalPosition>();
  // mixed coordinate systems are a hard failure
  if (prior_spherical != next_spherical) {
    throw(std::runtime_error(
        "Mixed coordinate systems in blocks referred to by single AudioChannelFormat not supported"));
  }

  auto block = std::invoke([next_spherical, &prior, &next, t]() {
    if (next_spherical) {
      return adm::AudioBlockFormatObjects(
          lerped_position(prior->get<adm::SphericalPosition>(), next.get<adm::SphericalPosition>(), t));
    } else {
      return adm::AudioBlockFormatObjects(
          lerped_position(prior->get<adm::CartesianPosition>(), next.get<adm::CartesianPosition>(), t));
    }
  });
  if (auto gain = lerp_if_required<adm::Gain>(*prior, next, t)) {
    block.set(*gain);
  }
  if (auto width = lerp_if_required<adm::Width>(*prior, next, t)) {
    block.set(*width);
  }
  if (auto height = lerp_if_required<adm::Height>(*prior, next, t)) {
    block.set(*height);
  }
  if (auto depth = lerp_if_required<adm::Depth>(*prior, next, t)) {
    block.set(*depth);
  }
  if (auto diffuse = lerp_if_required<adm::Diffuse>(*prior, next, t)) {
    block.set(*diffuse);
  }

  if (both_present_either_not_default<adm::ObjectDivergence>(*prior, next)) {
    auto prior_divergence = prior->get<adm::ObjectDivergence>();
    auto next_divergence = next.get<adm::ObjectDivergence>();
    auto divergence = prior_divergence;
    if (auto azimuth_range = lerp_if_required<adm::AzimuthRange>(prior_divergence, next_divergence, t)) {
      divergence.set(*azimuth_range);
    }
    if (auto position_range = lerp_if_required<adm::PositionRange>(prior_divergence, next_divergence, t)) {
      divergence.set(*position_range);
    }
    if (auto divergence_value = lerp_if_required<adm::Divergence>(prior_divergence, next_divergence, t)) {
      divergence.set(*divergence_value);
    }
    block.set(divergence);
  }

  return block;
}
}  // namespace

namespace eat::process {
std::pair<adm::AudioBlockFormatObjects, adm::AudioBlockFormatObjects> split(
    std::optional<adm::AudioBlockFormatObjects> const &prior_block, adm::AudioBlockFormatObjects const &block_to_split,
    adm::Rtime const &split_point) {
  auto original_duration = block_to_split.get<adm::Duration>().get();
  auto first_duration = eat::admx::minus(split_point.get(), block_to_split.get<adm::Rtime>().get());
  auto second_duration = eat::admx::minus(original_duration, first_duration);
  // second block has same end time as original, so same parameter values as original
  auto second_block(block_to_split);
  second_block.set(split_point);
  second_block.set(adm::Duration(second_duration));

  auto split_proportion = static_cast<float>(first_duration.asNanoseconds().count()) /
                          static_cast<float>(original_duration.asNanoseconds().count());
  if (split_proportion > 1.0f || split_proportion < 0.0f) {
    throw std::runtime_error("Cannot split block at rtime outside of block");
  }
  auto first_block = lerped_block(prior_block, block_to_split, split_proportion);
  first_block.set(adm::Rtime{block_to_split.get<adm::Rtime>()});
  first_block.set(adm::Duration{first_duration});
  return {first_block, second_block};
}

void clear_id(adm::AudioBlockFormatObjects &object) { object.set(adm::AudioBlockFormatId{}); }

namespace {
void copy_channels(adm::AudioPackFormat &pack, std::vector<std::shared_ptr<adm::AudioChannelFormat>> &channels) {
  auto const &cfs = pack.getReferences<adm::AudioChannelFormat>();
  std::copy(cfs.begin(), cfs.end(), std::back_inserter(channels));
}

template <typename ParentT>
bool copy_channels_from_pack_ref(ParentT &parent, std::vector<std::shared_ptr<adm::AudioChannelFormat>> &channels) {
  if (auto pf = parent.template getReference<adm::AudioPackFormat>()) {
    copy_channels(*pf, channels);
    return true;
  }
  return false;
}

template <typename ParentT>
bool copy_channel_from_ref(ParentT &parent, std::vector<std::shared_ptr<adm::AudioChannelFormat>> &channels) {
  if (auto cf = parent.template getReference<adm::AudioChannelFormat>()) {
    channels.push_back(std::move(cf));
    return true;
  }
  return false;
}

template <typename ParentT>
bool copy_from_channel_or_pack(ParentT &parent, std::vector<std::shared_ptr<adm::AudioChannelFormat>> &channels) {
  return copy_channel_from_ref(parent, channels) || copy_channels_from_pack_ref(parent, channels);
}

void add_referenced_channels(adm::AudioTrackFormat &tf,
                             std::vector<std::shared_ptr<adm::AudioChannelFormat>> &channels) {
  if (auto sf = tf.getReference<adm::AudioStreamFormat>()) {
    copy_from_channel_or_pack(*sf, channels);
  }
}
}  // namespace
std::vector<std::shared_ptr<adm::AudioChannelFormat>> referenced_channel_formats(adm::Document &doc) {
  std::vector<std::shared_ptr<adm::AudioChannelFormat>> channelFormats;
  for (auto uids = doc.getElements<adm::AudioTrackUid>(); auto const &uid : uids) {
    if (!copy_from_channel_or_pack(*uid, channelFormats)) {
      if (auto tf = uid->getReference<adm::AudioTrackFormat>()) {
        add_referenced_channels(*tf, channelFormats);
      }
    }
  }
  std::sort(channelFormats.begin(), channelFormats.end());
  channelFormats.erase(std::unique(channelFormats.begin(), channelFormats.end()), channelFormats.end());
  return channelFormats;
}

std::vector<std::shared_ptr<adm::AudioChannelFormat>> only_object_type(
    const std::vector<std::shared_ptr<adm::AudioChannelFormat>> &input) {
  std::vector<std::shared_ptr<adm::AudioChannelFormat>> filtered;
  std::copy_if(input.begin(), input.end(), std::back_inserter(filtered),
               [](std::shared_ptr<adm::AudioChannelFormat const> const &cf) {
                 return cf->get<adm::TypeDescriptor>() == adm::TypeDefinition::OBJECTS;
               });
  return filtered;
}
}  // namespace eat::process
