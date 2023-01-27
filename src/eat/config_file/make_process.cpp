#include <ear/bs2051.hpp>
#include <map>

#include "eat/process/adm_bw64.hpp"
#include "eat/process/block_resampling.hpp"
#include "eat/process/block_subelement_dropper.hpp"
#include "eat/process/jump_position_removal.hpp"
#include "eat/process/limit_interaction.hpp"
#include "eat/process/loudness.hpp"
#include "eat/process/misc.hpp"
#include "eat/process/profile_conversion_misc.hpp"
#include "eat/process/remove_elements.hpp"
#include "eat/process/remove_unused.hpp"
#include "eat/process/validate_process.hpp"
#include "eat/render/render.hpp"
#include "eat/utilities/parse_id_variant.hpp"
#include "make_graph.hpp"
#include "utilities.hpp"

namespace eat::process {

inline void from_json(nlohmann::json const &config, Constraint &constraint) {
  if (config.contains("min")) {
    config["min"].get_to(constraint.min);
  }
  if (config.contains("max")) {
    config["max"].get_to(constraint.max);
  }
}

inline void from_json(nlohmann::json const &config, PositionConstraint &constraint) {
  if (config.contains("min")) {
    constraint.min = config["min"].get<Constraint>();
  }
  if (config.contains("max")) {
    constraint.max = config["max"].get<Constraint>();
  }
  if (config.contains("permitted")) {
    constraint.permitted = config["permitted"].get<bool>();
  }
}

inline void from_json(nlohmann::json const &config, PositionInteractionConstraint &constraints) {
  if (config.contains("azimuth")) {
    constraints.azimuth = config["azimuth"].get<PositionConstraint>();
  }
  if (config.contains("elevation")) {
    constraints.elevation = config["elevation"].get<PositionConstraint>();
  }
  if (config.contains("distance")) {
    constraints.distance = config["distance"].get<PositionConstraint>();
  }
  if (config.contains("X")) {
    constraints.x = config["X"].get<PositionConstraint>();
  }
  if (config.contains("Y")) {
    constraints.y = config["Y"].get<PositionConstraint>();
  }
  if (config.contains("Z")) {
    constraints.z = config["Z"].get<PositionConstraint>();
  }
}

inline float linearFromJsonGain(nlohmann::json const &json_gain) {
  auto gain = json_gain.at("gain").get<float>();
  if (json_gain.contains("unit") && json_gain.at("unit").get<std::string>() == "dB") {
    return adm::Gain::fromDb(gain).asLinear();
  }
  return gain;
}

inline Constraint parseGainConstraint(nlohmann::json const &config) {
  Constraint constraint;
  if (config.contains("min")) {
    constraint.min = linearFromJsonGain(config["min"]);
  }
  if (config.contains("max")) {
    constraint.max = linearFromJsonGain(config["max"]);
  }
  return constraint;
}

inline void from_json(nlohmann::json const &config, GainInteractionConstraint &constraints) {
  if (config.contains("min")) {
    constraints.min = parseGainConstraint(config["min"]);
  }
  if (config.contains("max")) {
    constraints.max = parseGainConstraint(config["max"]);
  }
  if (config.contains("permitted")) {
    constraints.permitted = config["permitted"].get<bool>();
  }
}
}  // namespace eat::process

namespace eat::config_file {
namespace {

/// make a function which makes a process which takes no arguments
template <typename CB>
auto make_process_no_args(CB cb) {
  return [cb](nlohmann::json &, const std::string &name) { return cb(name); };
}

template <typename T>
auto make_process_no_args() {
  return [](nlohmann::json &, const std::string &name) { return std::make_shared<T>(name); };
}

framework::ProcessPtr make_read_adm(nlohmann::json &config, const std::string &name) {
  std::string path = get<std::string>(config, "path");

  return process::make_read_adm(name, path);
}

framework::ProcessPtr make_read_bw64(nlohmann::json &config, const std::string &name) {
  std::string path = get<std::string>(config, "path");
  size_t block_size = get<size_t>(config, "block_size", 1024);

  return process::make_read_bw64(name, path, block_size);
}

framework::ProcessPtr make_read_adm_bw64(nlohmann::json &config, const std::string &name) {
  std::string path = get<std::string>(config, "path");
  size_t block_size = get<size_t>(config, "block_size", 1024);

  return process::make_read_adm_bw64(name, path, block_size);
}

framework::ProcessPtr make_write_adm_bw64(nlohmann::json &config, const std::string &name) {
  std::string path = get<std::string>(config, "path");

  return process::make_write_adm_bw64(name, path);
}

framework::ProcessPtr make_write_bw64(nlohmann::json &config, const std::string &name) {
  std::string path = get<std::string>(config, "path");

  return process::make_write_bw64(name, path);
}

framework::ProcessPtr make_remove_elements(nlohmann::json &config, const std::string &name) {
  auto id_strings = get<std::vector<std::string>>(config, "ids");
  process::ElementIds ids;
  for (auto &id_str : id_strings) ids.push_back(utilities::parse_id_variant(id_str));

  return process::make_remove_elements(name, ids);
}

process::profiles::Profile parse_profile(nlohmann::json &config) {
  auto type_str = get<std::string>(config, "type");
  if (type_str == "itu_emission") {
    int level = get<int>(config, "level");
    return process::profiles::ITUEmissionProfile{level};
  }

  throw std::runtime_error{"unknown profile type " + type_str};
}

framework::ProcessPtr make_validate(nlohmann::json &config, const std::string &name) {
  auto profile_json = get_subobject(config, "profile");

  auto profile = parse_profile(profile_json);
  check_empty(profile_json);

  return process::make_validate(name, profile);
}

framework::ProcessPtr make_render(nlohmann::json &config, const std::string &name) {
  auto layout_name = get<std::string>(config, "layout");
  auto layout = ear::getLayout(layout_name);
  size_t block_size = get<size_t>(config, "block_size", 1024);

  return render::make_render(name, layout, block_size);
}

framework::ProcessPtr make_measure_loudness(nlohmann::json &config, const std::string &name) {
  auto layout_name = get<std::string>(config, "layout");
  auto layout = ear::getLayout(layout_name);

  return process::make_measure_loudness(name, layout);
}

framework::ProcessPtr make_set_programme_loudness(nlohmann::json &config, const std::string &name) {
  std::string id_str = get<std::string>(config, "id");
  auto id = adm::parseAudioProgrammeId(id_str);

  return process::make_set_programme_loudness(name, id);
}

framework::ProcessPtr make_set_profiles(nlohmann::json &config, const std::string &name) {
  auto profiles_json = get<std::vector<nlohmann::json>>(config, "profiles");

  std::vector<process::profiles::Profile> profiles;
  for (auto &profile_json : profiles_json) {
    auto profile = parse_profile(profile_json);
    check_empty(profile_json);
    profiles.push_back(std::move(profile));
  }

  return process::make_set_profiles(name, profiles);
}

framework::ProcessPtr make_set_block_resampler(nlohmann::json &config, const std::string &name) {
  auto time = get<std::string>(config, "min_duration");
  return process::make_block_resampler(name, time);
}

framework::ProcessPtr make_drop_block_subelements(nlohmann::json &config, const ::std::string &name) {
  return process::make_block_subelement_dropper(
      name, process::parse_droppable(get<std::vector<std::string>>(config, "objects_subelements")));
}

framework::ProcessPtr make_rewrite_content_objects_emission(nlohmann::json &config, const std::string &name) {
  auto max_objects_depth = get<int>(config, "max_objects_depth", 2);
  return process::make_rewrite_content_objects_emission(name, max_objects_depth);
}

framework::ProcessPtr make_set_version(nlohmann::json &config, const std::string &name) {
  return process::make_set_version(name, get<std::string>(config, "version"));
}

framework::ProcessPtr make_limit_interaction(nlohmann::json &config, const std::string &name) {
  std::optional<process::PositionInteractionConstraint> position_limits;
  position_limits = get_optional<process::PositionInteractionConstraint>(config, "position_range");

  std::optional<process::GainInteractionConstraint> gain_limits;
  gain_limits = get_optional<process::GainInteractionConstraint>(config, "gain_range");

  bool remove_disabled_ranges = false;
  if (config.contains("remove_disabled_ranges")) {
    remove_disabled_ranges = get<bool>(config, "remove_disabled_ranges");
  }

  std::vector<process::InteractionLimiter::Droppable> types_to_disable;

  if (config.contains("disable_interaction_type")) {
    auto types = get<std::vector<std::string>>(config, "disable_interaction_type");
    std::transform(types.begin(), types.end(), std::back_inserter(types_to_disable), [](std::string const &str) {
      using enum process::InteractionLimiter::Droppable;
      if (str == "onOff") return OnOff;
      if (str == "gain") return Gain;
      if (str == "position") return Position;
      throw std::runtime_error(
          "Config error: " + str +
          " is not a valid interaction type string, valid types are \"onOff\", \"gain\", and \"position\"");
    });
  }

  process::InteractionLimiter::Config limiter_config{remove_disabled_ranges, gain_limits, position_limits,
                                                     types_to_disable};

  return std::make_shared<process::InteractionLimiter>(name, limiter_config);
}

}  // namespace

framework::ProcessPtr make_process(nlohmann::json &config) {
  using CB = std::function<framework::ProcessPtr(nlohmann::json &, const std::string &)>;

  static std::map<std::string, CB> callbacks = {{
      {"read_adm", &make_read_adm},
      {"read_bw64", &make_read_bw64},
      {"read_adm_bw64", &make_read_adm_bw64},
      {"write_adm_bw64", &make_write_adm_bw64},
      {"write_bw64", &make_write_bw64},
      {"remove_unused", make_process_no_args(&process::make_remove_unused)},
      {"remove_unused_elements", make_process_no_args(&process::make_remove_unused_elements)},
      {"remove_elements", &make_remove_elements},
      {"validate", &make_validate},
      {"fix_ds_frequency", make_process_no_args(&process::make_fix_ds_frequency)},
      {"fix_block_durations", make_process_no_args(&process::make_fix_block_durations)},
      {"fix_stream_pack_refs", make_process_no_args(&process::make_fix_stream_pack_refs)},
      {"convert_track_stream_to_channel", make_process_no_args(&process::make_convert_track_stream_to_channel)},
      {"add_block_rtimes", make_process_no_args(&process::make_add_block_rtimes)},
      {"render", &make_render},
      {"measure_loudness", &make_measure_loudness},
      {"set_programme_loudness", &make_set_programme_loudness},
      {"update_all_programme_loudnesses", make_process_no_args(&process::make_update_all_programme_loudnesses)},
      {"set_profiles", &make_set_profiles},
      {"set_position_defaults", make_process_no_args(&process::make_set_position_defaults)},
      {"remove_silent_atu", make_process_no_args(&process::make_remove_silent_atu)},
      {"resample_blocks", &make_set_block_resampler},
      {"remove_jump_position", make_process_no_args(&process::make_jump_position_remover)},
      {"remove_object_times_data_safe", make_process_no_args(&process::make_remove_object_times_data_safe)},
      {"remove_object_times_common_unsafe", make_process_no_args(&process::make_remove_object_times_common_unsafe)},
      {"remove_importance", make_process_no_args(&process::make_remove_importance)},
      {"drop_blockformat_subelements", &make_drop_block_subelements},
      {"rewrite_content_objects_emission", &make_rewrite_content_objects_emission},
      {"infer_object_interact", make_process_no_args(&process::make_infer_object_interact)},
      {"set_version", &make_set_version},
      {"set_content_dialogue_default", make_process_no_args(&process::make_set_content_dialogue_default)},
      {"limit_interaction", &make_limit_interaction},
  }};

  std::string type = get<std::string>(config, "type");
  std::string name = get<std::string>(config, "name");
  auto parameters = get<nlohmann::json>(config, "parameters", nlohmann::json::value_t::object);

  if (auto it = callbacks.find(type); it != callbacks.end()) {
    auto process = it->second(parameters, name);
    check_empty(parameters);
    return process;
  } else
    throw std::runtime_error{"unknown type: " + type};
}

}  // namespace eat::config_file
