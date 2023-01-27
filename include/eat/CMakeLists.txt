# While it is not required for headers to be added to a target for the target to
# build, if you fail to do so then some IDEs may not show the headers as part of
# a project or correctly parse them with their code model
target_sources(
  eat
  PRIVATE config_file/validate_config.hpp
          framework/evaluate.hpp
          framework/exceptions.hpp
          framework/process.hpp
          framework/utility_processes.hpp
          framework/value_ptr.hpp
          process/adm_bw64.hpp
          process/adm_time_extras.hpp
          process/block.hpp
          process/block_modification.hpp
          process/block_resampling.hpp
          process/block_subelement_dropper.hpp
          process/channel_mapping.hpp
          process/chna.hpp
          process/directspeaker_conversion.hpp
          process/jump_position_removal.hpp
          process/language_codes.hpp
          process/limit_interaction.hpp
          process/loudness.hpp
          process/misc.hpp
          process/profile_conversion_misc.hpp
          process/profiles.hpp
          process/remove_elements.hpp
          process/remove_unused.hpp
          process/silence_detect.hpp
          process/temp_dir.hpp
          process/validate_detail.hpp
          process/validate.hpp
          process/validate_process.hpp
          render/render.hpp
          render/rendering_items.hpp
          render/rendering_items_options_by_id.hpp
          testing/files.hpp
          utilities/element_visitor.hpp
          utilities/for_each_element.hpp
          utilities/for_each_reference.hpp
          utilities/parse_id_variant.hpp
          utilities/to_dot.hpp
          utilities/unwrap_named.hpp
          utilities/unwrap_shared.hpp)
