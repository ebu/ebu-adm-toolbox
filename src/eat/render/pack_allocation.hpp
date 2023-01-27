#pragma once

#include <adm/document.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "rendering_items_common.hpp"

namespace eat::render {

template <typename T>
using Ref = std::shared_ptr<const T>;

/// Represents a channel to allocate within an AllocationPack
struct AllocationChannel {
  /// channel format to match with channel_format in AllocationTrack
  ChannelFmtPointer channel_format;
  /// nested audioPackFormats which lead from the root audioPackFormat to this
  /// channel. pack_format in the AllocationTrack allocated to this channel
  /// must reference one of these
  std::vector<PackFmtPointer> pack_formats;
};

/// Represents a complete audioPackFormat to be allocated
struct AllocationPack {
  /// the top level audioPackFormat which references all channels to be allocated
  PackFmtPointer root_pack;
  /// channels to allocate within this pack, with the nested audioPackFormat
  /// structure which must be satisfied
  std::vector<AllocationChannel> channels;
};

/// Represents a track to be allocated
struct AllocationTrack {
  /// channelFormat referenced indirectly by this track
  ChannelFmtPointer channel_format;
  /// packFormat referenced by this track
  PackFmtPointer pack_format;

  AllocationTrack(ChannelFmtPointer channel_format_, PackFmtPointer pack_format_) noexcept
      : channel_format(channel_format_), pack_format(pack_format_) {}
  virtual ~AllocationTrack() {}
};

/// Represents an allocated pack format with the resulting association between channels and tracks
struct AllocatedPack {
  /// reference to the allocated pack for these channels; note that it's the
  /// AllocationPack not the AudioPackFormat which is referenced to allow for
  /// packs to be duplicated with different sets of channels to allocate (for
  /// matrix).
  Ref<AllocationPack> pack;
  /// the corresponding track allocated to each channel in pack.channels, or
  /// empty for a silent track
  std::vector<Ref<AllocationTrack>> allocation;
};

using Allocation = std::vector<AllocatedPack>;

std::vector<Allocation> allocate_packs(std::vector<Ref<AllocationPack>> packs, std::vector<Ref<AllocationTrack>> tracks,
                                       std::optional<std::vector<PackFmtPointer>> pack_refs, size_t num_silent_tracks,
                                       size_t max_results = 2);

}  // namespace eat::render
