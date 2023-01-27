#pragma once

#include <adm/elements/audio_object_interaction.hpp>
#include <numeric>
#include <optional>

#include "eat/process/adm_bw64.hpp"

namespace eat::process {

struct Constraint {
  float min{std::numeric_limits<float>::min()};
  float max{std::numeric_limits<float>::max()};
};

struct GainInteractionConstraint {
  std::optional<Constraint> min;
  std::optional<Constraint> max;
  bool permitted{true};
};

struct PositionConstraint {
  std::optional<Constraint> min;
  std::optional<Constraint> max;
  bool permitted{true};
};

struct PositionInteractionConstraint {
  PositionConstraint azimuth;
  PositionConstraint elevation;
  PositionConstraint distance;
  PositionConstraint x;
  PositionConstraint y;
  PositionConstraint z;
};

class InteractionLimiter : public framework::FunctionalAtomicProcess {
 public:
  enum class Droppable { OnOff, Gain, Position };
  struct Config {
    bool remove_disabled_ranges{false};
    std::optional<GainInteractionConstraint> gain_range;
    std::optional<PositionInteractionConstraint> position_range;
    std::vector<Droppable> types_to_disable;
  };
  explicit InteractionLimiter(std::string const &name, Config config);
  void process() override;

 private:
  framework::DataPortPtr<ADMData> in_axml;
  framework::DataPortPtr<ADMData> out_axml;
  Config config;
};

}  // namespace eat::process
