#include "eat/process/limit_interaction.hpp"

#include <adm/adm.hpp>

namespace {
using namespace adm;
static Gain second_using_firsts_representation(Gain const &first, Gain const &second) {
  if (first.isDb()) {
    return Gain::fromDb(second.asDb());
  } else {
    return Gain::fromLinear(second.asLinear());
  }
}

template <typename ParamT>
ParamT constrain_to_range(ParamT const &input, eat::process::Constraint range) {
  if (input.get() < range.min) {
    return ParamT(range.min);
  } else if (input.get() > range.max) {
    return ParamT(range.max);
  }
  return input;
}

static Gain constrain_to_range(Gain const &input, eat::process::Constraint range) {
  if (input.asLinear() < range.min) {
    return second_using_firsts_representation(input, Gain(range.min));
  } else if (input.asLinear() > range.max) {
    return second_using_firsts_representation(input, Gain(range.max));
  }
  return input;
}

static GainInteractionRange constrain(GainInteractionRange range,
                                      eat::process::GainInteractionConstraint const &constraint) {
  if (range.has<GainInteractionMin>() && constraint.min) {
    range.set(GainInteractionMin{constrain_to_range(range.get<GainInteractionMin>().get(), *constraint.min)});
  }
  if (range.has<GainInteractionMax>() && constraint.max) {
    range.set(GainInteractionMax{constrain_to_range(range.get<GainInteractionMax>().get(), *constraint.max)});
  }
  return range;
}

static void remove_forbidden(adm::AudioObjectInteraction &interaction,
                             eat::process::GainInteractionConstraint const &limits) {
  if (!limits.permitted) {
    interaction.unset<adm::GainInteractionRange>();
  }
}

static void constrain_gain_interaction(adm::AudioObject &object,
                                       eat::process::GainInteractionConstraint const &limits) {
  auto interactObj = object.get<AudioObjectInteraction>();
  if (interactObj.has<GainInteractionRange>() && !interactObj.isDefault<GainInteractionRange>()) {
    remove_forbidden(interactObj, limits);
    interactObj.set(constrain(interactObj.get<adm::GainInteractionRange>(), limits));
    object.set(interactObj);
  }
}

static PositionInteractionRange constrain(PositionInteractionRange range,
                                          eat::process::PositionInteractionConstraint const &limits) {
  if (range.has<AzimuthInteractionMin>() && limits.azimuth.min) {
    range.set(constrain_to_range(range.get<AzimuthInteractionMin>(), *limits.azimuth.min));
  }
  if (range.has<AzimuthInteractionMax>() && limits.azimuth.max) {
    range.set(constrain_to_range(range.get<AzimuthInteractionMax>(), *limits.azimuth.max));
  }
  if (range.has<ElevationInteractionMin>() && limits.elevation.min) {
    range.set(constrain_to_range(range.get<ElevationInteractionMin>(), *limits.elevation.min));
  }
  if (range.has<ElevationInteractionMax>() && limits.elevation.max) {
    range.set(constrain_to_range(range.get<ElevationInteractionMax>(), *limits.elevation.max));
  }
  if (range.has<DistanceInteractionMin>() && limits.distance.min) {
    range.set(constrain_to_range(range.get<DistanceInteractionMin>(), *limits.distance.min));
  }
  if (range.has<DistanceInteractionMax>() && limits.distance.max) {
    range.set(constrain_to_range(range.get<DistanceInteractionMax>(), *limits.distance.max));
  }
  if (range.has<XInteractionMin>() && limits.x.min) {
    range.set(constrain_to_range(range.get<XInteractionMin>(), *limits.x.min));
  }
  if (range.has<XInteractionMax>() && limits.x.max) {
    range.set(constrain_to_range(range.get<XInteractionMax>(), *limits.x.max));
  }
  if (range.has<YInteractionMin>() && limits.y.min) {
    range.set(constrain_to_range(range.get<YInteractionMin>(), *limits.y.min));
  }
  if (range.has<YInteractionMax>() && limits.y.max) {
    range.set(constrain_to_range(range.get<YInteractionMax>(), *limits.y.max));
  }
  if (range.has<ZInteractionMin>() && limits.z.min) {
    range.set(constrain_to_range(range.get<ZInteractionMin>(), *limits.z.min));
  }
  if (range.has<ZInteractionMax>() && limits.z.max) {
    range.set(constrain_to_range(range.get<ZInteractionMax>(), *limits.z.max));
  }
  return range;
}

static PositionInteractionRange remove_forbidden(PositionInteractionRange range,
                                                 eat::process::PositionInteractionConstraint const &constraints) {
  if (!constraints.azimuth.permitted) {
    range.unset<adm::AzimuthInteractionMin>();
    range.unset<adm::AzimuthInteractionMax>();
  }
  if (!constraints.elevation.permitted) {
    range.unset<adm::ElevationInteractionMin>();
    range.unset<adm::ElevationInteractionMax>();
  }
  if (!constraints.distance.permitted) {
    range.unset<adm::DistanceInteractionMin>();
    range.unset<adm::DistanceInteractionMax>();
  }
  if (!constraints.x.permitted) {
    range.unset<adm::XInteractionMin>();
    range.unset<adm::XInteractionMax>();
  }
  if (!constraints.y.permitted) {
    range.unset<adm::YInteractionMin>();
    range.unset<adm::YInteractionMax>();
  }
  if (!constraints.z.permitted) {
    range.unset<adm::ZInteractionMin>();
    range.unset<adm::ZInteractionMax>();
  }
  return range;
}

static void disableInteractionTypes(adm::AudioObject &object,
                                    std::vector<eat::process::InteractionLimiter::Droppable> interactionTypes) {
  using enum eat::process::InteractionLimiter::Droppable;
  auto interaction = object.get<adm::AudioObjectInteraction>();
  for (auto type : interactionTypes) {
    if (type == OnOff) {
      interaction.set(adm::OnOffInteract{false});
    }
    if (type == Gain) {
      interaction.unset<adm::GainInteract>();
    }
    if (type == Position) {
      interaction.unset<adm::PositionInteract>();
    }
  }
  object.set(interaction);
}

static void removeDisabledRanges(adm::AudioObject &object) {
  auto interactObj = object.get<AudioObjectInteraction>();
  if (!interactObj.has<adm::GainInteract>() || interactObj.get<adm::GainInteract>() == false) {
    interactObj.unset<adm::GainInteractionRange>();
  }
  if (!interactObj.has<adm::PositionInteract>() || interactObj.get<adm::PositionInteract>() == false) {
    interactObj.unset<adm::PositionInteractionRange>();
  }
  object.set(interactObj);
}

}  // namespace

namespace eat::process {
static void constrain_position_interaction(AudioObject &object, PositionInteractionConstraint const &limits) {
  auto interactObj = object.get<AudioObjectInteraction>();
  if (interactObj.has<PositionInteractionRange>() && !interactObj.isDefault<PositionInteractionRange>()) {
    auto constrained = remove_forbidden(interactObj.get<PositionInteractionRange>(), limits);
    constrained = constrain(constrained, limits);
    interactObj.set(std::move(constrained));
    object.set(std::move(interactObj));
  }
}

static void clear_interact_if_all_off(AudioObject &object) {
  if (object.has<Interact>() && object.has<AudioObjectInteraction>()) {
    auto interaction = object.get<AudioObjectInteraction>();
    if (!interaction.get<OnOffInteract>().get() &&
        (!interaction.has<GainInteract>() || !interaction.get<GainInteract>().get()) &&
        (!interaction.has<PositionInteract>() || !interaction.get<PositionInteract>().get())) {
      object.unset<Interact>();
      object.unset<AudioObjectInteraction>();
    }
  }
}

InteractionLimiter::InteractionLimiter(const std::string &name, Config config_)
    : framework::FunctionalAtomicProcess{name},
      in_axml(add_in_port<framework::DataPort<ADMData>>("in_axml")),
      out_axml(add_out_port<framework::DataPort<ADMData>>("out_axml")),
      config(config_) {}

void InteractionLimiter::process() {
  auto adm = std::move(in_axml->get_value());
  auto input_doc = adm.document.move_or_copy();
  for (auto &object : input_doc->getElements<adm::AudioObject>()) {
    if (object->has<AudioObjectInteraction>()) {
      disableInteractionTypes(*object, config.types_to_disable);
      if (config.remove_disabled_ranges) removeDisabledRanges(*object);
      clear_interact_if_all_off(*object);
      if (config.gain_range) constrain_gain_interaction(*object, *config.gain_range);
      if (config.position_range) constrain_position_interaction(*object, *config.position_range);
    }
  }
  adm.document = std::move(input_doc);
  out_axml->set_value(std::move(adm));
}

}  // namespace eat::process
