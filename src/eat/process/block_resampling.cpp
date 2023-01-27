//
// Created by Richard Bailey on 27/09/2022.
//

#include "eat/process/block_resampling.hpp"

#include <adm/document.hpp>
#include <algorithm>
#include <optional>
#include <tuple>

#include "eat/process/adm_time_extras.hpp"
#include "eat/process/block_modification.hpp"

namespace {
class Group {
 public:
  explicit Group(adm::Time interval_)
      : dur(std::invoke([&interval_]() {
          if (interval_.isNanoseconds()) {
            return adm::Time{std::chrono::nanoseconds::zero()};
          } else {
            return adm::Time{adm::FractionalTime(0, interval_.asFractional().denominator())};
          }
        })),
        interval(interval_) {}

  void add(adm::AudioBlockFormatObjects object) {
    using eat::admx::plus;
    dur = plus(dur, object.get<adm::Duration>().get());
    blocks.push_back(std::move(object));
  }

  [[nodiscard]] adm::Time duration() const { return dur; }

  [[nodiscard]] adm::Time remaining() const {
    using namespace std::chrono_literals;
    auto subbed = eat::admx::minus(interval, dur);
    if (subbed.asNanoseconds() < 0ns) {
      if (subbed.isNanoseconds()) {
        return adm::Time{std::chrono::nanoseconds::zero()};
      } else {
        return adm::Time{adm::FractionalTime{0, subbed.asFractional().denominator()}};
      }
    } else {
      return subbed;
    }
  };

  [[nodiscard]] adm::Rtime rtime() const {
    if (blocks.empty()) throw std::runtime_error("Can't return rtime of empty group");
    return blocks.back().get<adm::Rtime>();
  }

  [[nodiscard]] bool empty() const { return blocks.empty(); }

  [[nodiscard]] bool can_fit(adm::AudioBlockFormatObjects const &object) const {
    return eat::admx::plus(dur, object.get<adm::Duration>().get()).asNanoseconds() <= interval.asNanoseconds();
  }

  adm::AudioBlockFormatObjects thrifty_add(adm::AudioBlockFormatObjects const &block_to_split,
                                           std::optional<adm::AudioBlockFormatObjects> const &prior_block) {
    using eat::admx::minus;
    auto split_rtime = minus(last().get<adm::Rtime>().get(), remaining());
    auto [first, second] = eat::process::split(prior_block, block_to_split, adm::Rtime(split_rtime));
    add(second);
    return first;
  }

  adm::AudioBlockFormatObjects greedy_add(adm::AudioBlockFormatObjects const &block, adm::Time const &available_time,
                                          std::optional<adm::AudioBlockFormatObjects> const &prior) {
    auto remainder_duration = eat::admx::minus(interval, available_time);
    auto remainder_rtime = block.get<adm::Rtime>();
    auto rtime = adm::Rtime(eat::admx::plus(remainder_rtime.get(), remainder_duration));
    auto [first, second] = eat::process::split(prior, block, rtime);
    add(second);
    return first;
  }

  [[nodiscard]] adm::AudioBlockFormatObjects last() const {
    if (empty()) throw std::runtime_error("Can't return last element of empty group");
    return blocks.back();
  }

  [[nodiscard]] std::vector<adm::AudioBlockFormatObjects> const &data() const { return blocks; }

  [[nodiscard]] adm::Time min_duration() const { return interval; }

 private:
  std::vector<adm::AudioBlockFormatObjects> blocks;
  adm::Time dur;
  adm::Time interval;
};

class BlockStack {
 public:
  BlockStack(std::vector<adm::AudioBlockFormatObjects>::iterator begin,
             std::vector<adm::AudioBlockFormatObjects>::iterator end)
      : stack{begin, end} {
    for (auto const &block : stack) {
      dur += block.get<adm::Duration>()->asNanoseconds();
    }
  }

  void push(adm::AudioBlockFormatObjects block) {
    dur += block.get<adm::Duration>()->asNanoseconds();
    stack.push_back(std::move(block));
  }

  std::optional<adm::AudioBlockFormatObjects> pop() {
    if (!empty()) {
      dur -= stack.back().get<adm::Duration>()->asNanoseconds();
      auto block = stack.back();
      stack.pop_back();
      return block;
    }
    return {};
  }

  [[nodiscard]] bool empty() const { return stack.empty(); }

  [[nodiscard]] adm::Time duration() const { return {dur}; }

  [[nodiscard]] adm::AudioBlockFormatObjects const &top() const { return stack.back(); }

 private:
  std::vector<adm::AudioBlockFormatObjects> stack;
  std::chrono::nanoseconds dur{};
};

enum class GroupSplitPolicy {
  SplitGreedy,
  SplitThrifty,
  ConsumeOne,
  ConsumeAll,
};

bool isLargeBlock(adm::AudioBlockFormatObjects const &block, adm::Time const &interval) {
  return block.get<adm::Duration>()->asNanoseconds() > interval.asNanoseconds();
}

GroupSplitPolicy large_block_policy(Group const &group, BlockStack const &stack) {
  auto const &nextBlock = stack.top();
  auto const block_duration = nextBlock.get<adm::Duration>().get();
  auto const stack_duration = stack.duration();
  auto const min_group_duration = group.min_duration();
  auto const remaining_stack_after_pop = eat::admx::minus(stack_duration, block_duration);

  using namespace std::chrono_literals;
  if (remaining_stack_after_pop.asNanoseconds() > 0ms &&
      remaining_stack_after_pop.asNanoseconds() < min_group_duration.asNanoseconds()) {
    return GroupSplitPolicy::SplitGreedy;
  } else {
    return GroupSplitPolicy::ConsumeOne;
  }
}

GroupSplitPolicy normal_block_policy(Group const &group, BlockStack const &stack) {
  if (stack.duration().asNanoseconds() >= 2 * group.min_duration().asNanoseconds()) {
    return GroupSplitPolicy::SplitThrifty;
  }
  return GroupSplitPolicy::ConsumeAll;
}

GroupSplitPolicy group_policy(Group const &group, BlockStack const &stack) {
  using namespace std::chrono_literals;
  auto const &nextBlock = stack.top();
  if (isLargeBlock(nextBlock, group.min_duration())) {
    return large_block_policy(group, stack);
  } else {
    return normal_block_policy(group, stack);
  }
}

struct LastParameter {
  std::optional<adm::SphericalPosition> spherical_position;
  std::optional<adm::CartesianPosition> cartesian_position;
  std::optional<adm::Gain> gain;
  std::optional<adm::Width> width;
  std::optional<adm::Height> height;
  std::optional<adm::Depth> depth;
  std::optional<adm::Diffuse> diffuse;
  std::optional<adm::ObjectDivergence> object_divergence;
};

template <typename AttT, typename ParentT>
bool set_and_not_default(ParentT const &p) {
  return p.template has<AttT>() && !p.template isDefault<AttT>();
}

template <typename AttT, typename ParentT>
std::optional<AttT> get_if_set_not_default(ParentT const &parent) {
  if (set_and_not_default<AttT>(parent)) return parent.template get<AttT>();
  return {};
}

std::tuple<std::optional<adm::Azimuth>, std::optional<adm::Elevation>, std::optional<adm::Distance>>
retrieve_set_position_elements(adm::SphericalPosition const &pos) {
  return {get_if_set_not_default<adm::Azimuth>(pos), get_if_set_not_default<adm::Elevation>(pos),
          get_if_set_not_default<adm::Distance>(pos)};
}

std::tuple<std::optional<adm::X>, std::optional<adm::Y>, std::optional<adm::Z>> retrieve_set_position_elements(
    adm::CartesianPosition const &pos) {
  return {get_if_set_not_default<adm::X>(pos), get_if_set_not_default<adm::Y>(pos),
          get_if_set_not_default<adm::Z>(pos)};
}

template <typename AttT, typename ParentT>
void set_if_present(std::optional<AttT> const &att, ParentT &parent) {
  if (att) {
    parent.set(*att);
  }
}

LastParameter getLastParameters(Group const &group) {
  auto const &blocks = group.data();
  LastParameter last;
  for (auto i = blocks.rbegin(); i != blocks.rend(); ++i) {
    if (i->has<adm::SphericalPosition>()) {
      if (!last.spherical_position)
        last.spherical_position = i->get<adm::SphericalPosition>();
      else {
        auto [az, el, d] = retrieve_set_position_elements(i->get<adm::SphericalPosition>());
        set_if_present(az, *last.spherical_position);
        set_if_present(el, *last.spherical_position);
        set_if_present(d, *last.spherical_position);
      }
    }
    if (i->has<adm::CartesianPosition>()) {
      if (!last.cartesian_position)
        last.cartesian_position = i->get<adm::CartesianPosition>();
      else {
        auto [x, y, z] = retrieve_set_position_elements(i->get<adm::CartesianPosition>());
        set_if_present(x, *last.cartesian_position);
        set_if_present(y, *last.cartesian_position);
        set_if_present(z, *last.cartesian_position);
      }
    }
    if (auto gain = get_if_set_not_default<adm::Gain>(*i)) {
      last.gain = gain;
    }
    if (auto width = get_if_set_not_default<adm::Width>(*i)) {
      last.width = width;
    }
    if (auto height = get_if_set_not_default<adm::Height>(*i)) {
      last.height = height;
    }
    if (auto depth = get_if_set_not_default<adm::Depth>(*i)) {
      last.depth = depth;
    }
    if (auto diffuse = get_if_set_not_default<adm::Diffuse>(*i)) {
      last.diffuse = diffuse;
    }
    if (auto object_divergence = get_if_set_not_default<adm::ObjectDivergence>(*i)) {
      last.object_divergence = object_divergence;
    }
  }
  return last;
}

adm::AudioBlockFormatObjects create_block(LastParameter const &parameters) {
  auto block = std::invoke([&parameters]() {
    if (parameters.cartesian_position) {
      return adm::AudioBlockFormatObjects{*parameters.cartesian_position};
    } else {
      assert(parameters.spherical_position);
      return adm::AudioBlockFormatObjects{*parameters.spherical_position};
    }
  });
  if (parameters.gain) {
    block.set(*parameters.gain);
  }
  if (parameters.width) {
    block.set(*parameters.width);
  }
  if (parameters.height) {
    block.set(*parameters.height);
  }
  if (parameters.depth) {
    block.set(*parameters.depth);
  }
  if (parameters.diffuse) {
    block.set(*parameters.diffuse);
  }
  if (parameters.object_divergence) {
    block.set(*parameters.object_divergence);
  }
  return block;
}

adm::AudioBlockFormatObjects group_to_block(Group const &group) {
  auto parameters = getLastParameters(group);
  auto consolidated = create_block(parameters);
  consolidated.set(group.rtime());
  consolidated.set(adm::Duration(group.duration()));
  return consolidated;
}

Group next_group(BlockStack &remaining, adm::Time const &interval,
                 std::optional<adm::AudioBlockFormatObjects> const &leading_zero_length) {
  Group group(interval);
  auto policy = group_policy(group, remaining);
  switch (policy) {
    case GroupSplitPolicy::ConsumeOne: {
      assert(!remaining.empty());
      group.add(*remaining.pop());
      return group;
    }
    case GroupSplitPolicy::SplitGreedy: {
      // Need to do this in two steps as stack duration *after* stack pop is required for greedy_add
      // and order of function argument evaluation is undefined
      auto next_block = remaining.pop();
      assert(next_block);
      std::optional<adm::AudioBlockFormatObjects> prior;
      if (!remaining.empty()) {
        prior = remaining.top();
      } else if (leading_zero_length) {
        prior = leading_zero_length;
      }
      remaining.push(group.greedy_add(*next_block, remaining.duration(), prior));
      return group;
    }
    case GroupSplitPolicy::SplitThrifty: {
      auto next_block = remaining.pop();
      while (next_block && group.can_fit(*next_block)) {
        group.add(*next_block);
        next_block = remaining.pop();
      }
      if (next_block) {
        std::optional<adm::AudioBlockFormatObjects> prior;
        if (!remaining.empty()) {
          prior = remaining.top();
        } else if (leading_zero_length) {
          prior = leading_zero_length;
        }
        remaining.push(group.thrifty_add(*next_block, prior));
      }
      return group;
    }
    case GroupSplitPolicy::ConsumeAll: {
      auto next_block = remaining.pop();
      while (next_block) {
        group.add(*next_block);
        next_block = remaining.pop();
      }
      return group;
    }
    default: {
      // This is unreachable, but keeps GCC happy.
      assert(false);
      return group;
    }
  }
}

bool is_zero_length(adm::AudioBlockFormatObjects const &block) {
  using namespace std::chrono_literals;
  return block.has<adm::Duration>() && block.get<adm::Duration>()->asNanoseconds() == 0ns;
}

}  // namespace

namespace eat::process {

std::vector<adm::AudioBlockFormatObjects> resample_to_minimum_preserving_zero(
    adm::BlockFormatsRange<adm::AudioBlockFormatObjects> const blocks, adm::Time const &interval) {
  std::vector<adm::AudioBlockFormatObjects> resampled;
  if (blocks.empty() || blocks.size() == 1) {
    return std::vector<adm::AudioBlockFormatObjects>(blocks.begin(), blocks.end());
  }

  std::optional<adm::AudioBlockFormatObjects> leading_zero_length{};
  if (is_zero_length(blocks.front())) {
    leading_zero_length = blocks.front();
  }

  auto first = blocks.begin();
  if (leading_zero_length) {
    ++first;
  }

  BlockStack remaining(first, blocks.end());
  while (!remaining.empty()) {
    auto group = next_group(remaining, interval, leading_zero_length);
    resampled.push_back(group_to_block(group));
  }

  if (leading_zero_length) {
    resampled.push_back(*leading_zero_length);
  }

  std::reverse(resampled.begin(), resampled.end());

  for (auto &block : resampled) {
    clear_id(block);
  }

  return resampled;
}

std::vector<adm::AudioBlockFormatObjects> de_duplicate_zero_length_blocks(
    adm::BlockFormatsRange<adm::AudioBlockFormatObjects> blocks) {
  std::vector<adm::AudioBlockFormatObjects> de_duped(blocks.begin(), blocks.end());
  auto last = std::unique(de_duped.rbegin(), de_duped.rend(),
                          [](adm::AudioBlockFormatObjects const &lhs, adm::AudioBlockFormatObjects const &rhs) {
                            return is_zero_length(lhs) && is_zero_length(rhs);
                          });
  de_duped.erase(de_duped.begin(), last.base());
  return de_duped;
}

}  // namespace eat::process

namespace eat::process {

BlockResampler::BlockResampler(const std::string &name, adm::Time min_duration_)
    : framework::FunctionalAtomicProcess{name},
      in_axml(add_in_port<framework::DataPort<ADMData>>("in_axml")),
      out_axml(add_out_port<framework::DataPort<ADMData>>("out_axml")),
      min_duration(std::move(min_duration_)) {}

void BlockResampler::process() {
  auto adm = std::move(in_axml->get_value());
  auto input_doc = adm.document.move_or_copy();
  for (auto const &cf : only_object_type(referenced_channel_formats(*input_doc))) {
    auto resampled = resample_to_minimum_preserving_zero(cf->getElements<adm::AudioBlockFormatObjects>(), min_duration);
    cf->clearAudioBlockFormats();
    for (auto const &bf : resampled) {
      cf->add(bf);
    }
  }
  adm.document = std::move(input_doc);
  out_axml->set_value(std::move(adm));
}

framework::ProcessPtr make_block_resampler(const std::string &name, std::string const &min_duration) {
  return std::make_shared<BlockResampler>(name, adm::parseTimecode(min_duration));
}

}  // namespace eat::process
