#include "pack_allocation.hpp"

namespace eat::render {

// this module approximately corresponds with ear.core.select_items.pack_allocation
//
// the algorithm structure is a bit different, but it should meet the same
// requirements
//
// - rather than using coroutines/generators, results are given through a
//   callback, which can return true to search for more results or false to
//   cancel
//
// - less care is taken to avoid recursion (and copying the state). I think
//   this can be made efficient enough that it's not worth it, though it might
//   be sensible to use an explicit stack instead. The maximum stack depth
//   will be around the number of tracks plus the number of allocated packs.
//
// the algorithm operates recursively, trying out different packs and
// allocations of the tracks to channels stored in a TempSolution
//
// each recursive step tries different options for allocating the next track
// (tracks are allocated in order, real tracks first followed by silent
// tracks). A track can either be allocated to an un-allocated channel in an
// allocated pack, or to a channel in a new pack
//
// the recursion terminates if a solution is found (all tracks, channels and
// pack references allocated), or the solution is infeasible (we ran out of
// tracks before all channels are allocated, or no channel or new pack can
// satisfy the current track), or the search is cancelled by the solution
// callback
//
// once the silent tracks are reached, new packs can only be allocated once
// all channels in existing packs are allocated, to avoid duplicate solutions
// with packs allocated at different points
//
// possible improvements:
// - keep track of the number of unallocated channels in the state
//
// - keep track of the whether each pack is fully allocated
//
// - remove packs that couldn't possibly be allocated before starting to make
//   the state smaller
//
// - use a fancy memory allocation strategy, essentially building a stack on
//   the heap using std::pmr
//
// - don't allocate memory at all: there's no need to copy the state as long as
//   we can undo the changes we've made after recursion. in most cases this is
//   easy, though the structure required to do this in update_packs_possible
//   without allocation would be pretty odd

struct TempAllocatedPack {
  Ref<AllocationPack> pack;
  std::vector<Ref<AllocationTrack>> allocation;
  /// true if the corresponding allocation has been made
  /// false: unallocated
  /// true and allocation[i]: allocated to track
  /// true and not allocation[i]: allocated to silent
  std::vector<bool> allocated;

  TempAllocatedPack(Ref<AllocationPack> pack_)
      : pack(std::move(pack_)), allocation(pack->channels.size()), allocated(pack->channels.size(), false) {}

  bool complete() const {
    for (bool b : allocated)
      if (!b) return false;
    return true;
  }
};

// return true to continue
using AllocationCB = std::function<bool(Allocation)>;

struct Context {
  std::vector<Ref<AllocationPack>> packs;
  std::vector<Ref<AllocationTrack>> tracks;
  std::optional<std::vector<PackFmtPointer>> pack_refs;
  size_t num_silent_tracks;
  AllocationCB cb;
};

struct TempSolution {
  std::vector<bool> pack_ref_allocated;

  /// index of the next track to allocate, non-silent tracks first, from 0 to
  /// (tracks.size() + num_silent_tracks)
  size_t track_alloc_idx;

  std::vector<bool> pack_possible;

  std::vector<TempAllocatedPack> allocation;
};

static bool complete(const Context &ctx, const TempSolution &s) {
  if (s.track_alloc_idx < ctx.tracks.size() + ctx.num_silent_tracks) return false;

  for (bool b : s.pack_ref_allocated)
    if (!b) return false;

  for (const auto &alloc : s.allocation)
    if (!alloc.complete()) return false;

  return true;
}

static size_t get_silent_left(const Context &ctx, const TempSolution &s) {
  return ctx.num_silent_tracks - (std::max(ctx.tracks.size(), s.track_alloc_idx) - ctx.tracks.size());
}

static size_t get_tracks_left(const Context &ctx, const TempSolution &s) {
  return (ctx.num_silent_tracks + ctx.tracks.size()) - s.track_alloc_idx;
}

static bool current_track_silent(const Context &ctx, const TempSolution &s) {
  return s.track_alloc_idx >= ctx.tracks.size();
}

static bool track_possible(const AllocationChannel &channel, const Ref<AllocationTrack> &track) {
  if (channel.channel_format != track->channel_format) return false;

  for (const auto &possible_pack : channel.pack_formats)
    if (track->pack_format == possible_pack) return true;

  return false;
}

static bool allocate_packs(const Context &ctx, const TempSolution &s);

/// try to allocate the current track to a channel in the pack at alloc_pack_idx
/// found_channel is set to indicate whether any channel was found, because
/// when allocating silent tracks we must only try one channel
static bool try_alloc_track(const Context &ctx, const TempSolution &s, size_t alloc_pack_idx, bool &found_channel) {
  const TempAllocatedPack &alloc_pack = s.allocation.at(alloc_pack_idx);
  const Ref<AllocationPack> &pack = alloc_pack.pack;
  found_channel = false;

  if (current_track_silent(ctx, s)) {
    // we're into the silent tracks, so just allocate to the first available channel
    for (size_t channel_idx = 0; channel_idx < pack->channels.size(); channel_idx++)
      if (!alloc_pack.allocated.at(channel_idx)) {
        TempSolution sc = s;

        TempAllocatedPack &new_alloc_pack = sc.allocation.at(alloc_pack_idx);
        new_alloc_pack.allocated.at(channel_idx) = true;

        sc.track_alloc_idx++;

        found_channel = true;
        return allocate_packs(ctx, sc);
      }

  } else {
    // try each compatible channel
    for (size_t channel_idx = 0; channel_idx < pack->channels.size(); channel_idx++)
      if (!alloc_pack.allocated.at(channel_idx) &&
          track_possible(pack->channels.at(channel_idx), ctx.tracks.at(s.track_alloc_idx))) {
        TempSolution sc = s;

        TempAllocatedPack &new_alloc_pack = sc.allocation.at(alloc_pack_idx);
        new_alloc_pack.allocated.at(channel_idx) = true;
        new_alloc_pack.allocation.at(channel_idx) = ctx.tracks.at(s.track_alloc_idx);

        sc.track_alloc_idx++;

        found_channel = true;
        if (!allocate_packs(ctx, sc)) return false;
      }
  }

  return true;
}

/// try to allocate a track to any channel in any pack
static bool try_alloc_track(const Context &ctx, const TempSolution &s) {
  for (size_t alloc_idx = 0; alloc_idx < s.allocation.size(); alloc_idx++) {
    bool found_channel = false;
    if (!try_alloc_track(ctx, s, alloc_idx, found_channel)) return false;
    // if we're onto the silent tracks then stop once we've found a channel, as
    // otherwise we would get multiple equivalent solutions
    if (current_track_silent(ctx, s) && found_channel) break;
  }
  return true;
}

/// could the current track be allocated to pack if it were added to the
/// solution?
static bool pack_compatible_with_current_track(const Context &ctx, const TempSolution &s,
                                               const Ref<AllocationPack> &pack) {
  if (current_track_silent(ctx, s)) {
    // silent tracks can be allocated to any channel
    return true;
  } else {
    for (const auto &channel : pack->channels)
      if (track_possible(channel, ctx.tracks.at(s.track_alloc_idx))) return true;
    return false;
  }
}

// update s.pack_possible to reflect any changes in the current solution
//
// the logic in here is:
// - packs must not be marked as impossible if they might be (otherwise they
//   might not be allocated where they should be)
// - packs can be marked as possible if they might be impossible (as the rest
//   of the algorithm will figure out if they are not actually possible, and we
//   can't really know without doing all the allocation
// - it's good to mark packs as impossible as this reduces the search space,
//   but there's no point in duplicating the rest of the pack allocation logic
//   here
static void update_packs_possible(const Context &ctx, TempSolution &s) {
  // number of tracks left including silent
  const size_t total_tracks_left = get_tracks_left(ctx, s);
  // number of silent tracks left
  const size_t total_silent_left = get_silent_left(ctx, s);
  // number of channels in allocated packs which are yet to be allocated
  size_t unallocated_channels = 0;
  for (const auto &alloc : s.allocation)
    for (bool allocated : alloc.allocated)
      if (!allocated) unallocated_channels++;

  assert(unallocated_channels <= total_tracks_left);
  // number of silent or real tracks which will not be needed by the unallocated channels
  const size_t tracks_left = total_tracks_left - unallocated_channels;
  // max possible silent tracks left after all unallocated channels have been assigned
  const size_t max_silent_left = std::min(tracks_left, total_silent_left);

  for (size_t pack_idx = 0; pack_idx < ctx.packs.size(); pack_idx++) {
    bool possible = s.pack_possible.at(pack_idx);
    if (possible) {
      const Ref<AllocationPack> &pack = ctx.packs.at(pack_idx);

      // this pack is impossible if:
      // - there are pack refs but this pack is not referenced by an unallocated reference
      if (ctx.pack_refs) {
        bool found_compatible = false;
        for (size_t pack_ref_idx = 0; pack_ref_idx < ctx.pack_refs->size(); pack_ref_idx++)
          if (!s.pack_ref_allocated.at(pack_ref_idx) && ctx.pack_refs->at(pack_ref_idx) == pack->root_pack) {
            found_compatible = true;
            break;
          }

        if (!found_compatible) {
          s.pack_possible.at(pack_idx) = false;
          continue;
        }
      }

      // - there are more channels left than tracks
      if (pack->channels.size() > tracks_left) {
        s.pack_possible.at(pack_idx) = false;
        continue;
      }

      // - the number of channels that could only be allocated to silent tracks
      //   (because there are no compatible unallocated tracks) is greater than
      //   the maximum possible number of silent tracks left after the existing
      //   packs have been allocated
      size_t silent_required = 0;

      for (const auto &channel : pack->channels) {
        bool found_compatible = false;
        for (size_t track_idx = s.track_alloc_idx; track_idx < ctx.tracks.size(); track_idx++)
          if (track_possible(channel, ctx.tracks.at(track_idx))) {
            found_compatible = true;
            break;
          }

        if (!found_compatible) {
          silent_required++;
          if (silent_required > max_silent_left) {
            s.pack_possible.at(pack_idx) = false;
            break;
          }
        }
      }
    }
  }
}

/// try adding a pack then allocating a track to it
static bool try_alloc_new_pack(const Context &ctx, TempSolution s) {
  // don't allocate a pack if we're allocating silent tracks and there are any
  // channels left; duplicate solutions would be found with the packs added at
  // different points
  if (current_track_silent(ctx, s)) {
    bool channels_left = false;
    for (const auto &alloc : s.allocation)
      if (!alloc.complete()) {
        channels_left = true;
        break;
      }

    if (channels_left) return true;
  }

  update_packs_possible(ctx, s);

  for (size_t pack_idx = 0; pack_idx < ctx.packs.size(); pack_idx++)
    if (s.pack_possible.at(pack_idx) && pack_compatible_with_current_track(ctx, s, ctx.packs.at(pack_idx))) {
      TempSolution sc = s;

      // allocate pack
      sc.allocation.push_back(TempAllocatedPack(ctx.packs.at(pack_idx)));

      // mark the pack ref
      if (ctx.pack_refs) {
        for (size_t pack_ref_idx = 0; pack_ref_idx < ctx.pack_refs->size(); pack_ref_idx++)
          if (!sc.pack_ref_allocated.at(pack_ref_idx) &&
              ctx.pack_refs->at(pack_ref_idx) == ctx.packs.at(pack_idx)->root_pack) {
            sc.pack_ref_allocated.at(pack_ref_idx) = true;
            break;
          }
      }

      // allocate the channel
      bool found_channel = false;
      if (!try_alloc_track(ctx, sc, sc.allocation.size() - 1, found_channel)) return false;
      assert(found_channel);

      // if allocating new packs when there's only silent packs, don't try
      // different options as they would result in equivalent solutions
      if (current_track_silent(ctx, s)) break;
    }

  return true;
}

/// main recursive search step; this either calls the callback if the solution
/// is complete, fails if it's impossible, or tries adding a new pack
/// (try_alloc_new_pack) or allocating the current track (try_alloc_track)
static bool allocate_packs(const Context &ctx, const TempSolution &s) {
  if (complete(ctx, s)) {
    std::vector<AllocatedPack> allocation;
    for (const auto &temp_alloc : s.allocation) allocation.push_back({temp_alloc.pack, temp_alloc.allocation});
    return ctx.cb(allocation);
  } else if (s.track_alloc_idx >= ctx.num_silent_tracks + ctx.tracks.size()) {
    // no more tracks left but not complete -- this can never be a valid solution
    return true;
  } else {
    if (!try_alloc_new_pack(ctx, s)) return false;

    if (!try_alloc_track(ctx, s)) return false;

    return true;
  }
}

static void allocate_packs(std::vector<Ref<AllocationPack>> packs, std::vector<Ref<AllocationTrack>> tracks,
                           std::optional<std::vector<PackFmtPointer>> pack_refs, size_t num_silent_tracks,
                           const AllocationCB &cb) {
  Context ctx;
  ctx.packs = packs;
  ctx.tracks = tracks;
  ctx.pack_refs = pack_refs;
  ctx.num_silent_tracks = num_silent_tracks;
  ctx.cb = cb;

  TempSolution s;
  if (pack_refs) s.pack_ref_allocated = std::vector<bool>(pack_refs->size(), false);
  s.track_alloc_idx = 0;
  s.pack_possible = std::vector<bool>(packs.size(), true);

  allocate_packs(ctx, s);
}

std::vector<Allocation> allocate_packs(std::vector<Ref<AllocationPack>> packs, std::vector<Ref<AllocationTrack>> tracks,
                                       std::optional<std::vector<PackFmtPointer>> pack_refs, size_t num_silent_tracks,
                                       size_t max_results) {
  std::vector<Allocation> res;
  allocate_packs(packs, tracks, pack_refs, num_silent_tracks, [&res, max_results](Allocation allocation) {
    if (res.size() < max_results) {
      res.push_back(allocation);
      return true;
    } else
      return false;
  });
  return res;
}

}  // namespace eat::render
