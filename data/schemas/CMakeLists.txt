list(
  APPEND
  schemas
  config.schema.json
  ports.schema.json
  process.schema.json
  processes.schema.json
  drop_blockformat_subelements.schema.json
  layout_processes.schema.json
  limit_interaction.schema.json
  parameterless_processes.schema.json
  path_process.schema.json
  profile.schema.json
  remove_elements.schema.json
  resample_blocks.schema.json
  rewrite_content_objects_emission.schema.json
  set_profiles.schema.json
  set_programme_loudness.schema.json
  set_version.schema.json
  validate.schema.json)
set(config_src_dir "${PROJECT_BINARY_DIR}/src/eat/config_file/schemas")
foreach(schema ${schemas})
  string(REGEX REPLACE "\\..*" "" schema_path ${schema})
  string(REPLACE "/" "_" schema_name ${schema_path})
  set(header_name ${schema_name}.hpp)
  set(header_file "${CMAKE_CURRENT_BINARY_DIR}/${header_name}")
  # This runs at build time rather than configure time, and only runs if the
  # file doesn't exist or the schema has changed
  add_custom_command(
    OUTPUT ${header_file}
    COMMENT data/schemas/generate_schema.cmake
    COMMAND
      ${CMAKE_COMMAND} -DSCHEMA_NAME:STRING=${schema_name}
      -DSCHEMA_HEADER:FILEPATH=${header_file}
      -DSCHEMA:FILEPATH=${CMAKE_CURRENT_SOURCE_DIR}/${schema} -V -P
      ${CMAKE_CURRENT_SOURCE_DIR}/generate_schema.cmake
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${schema}
    VERBATIM)
  list(APPEND header_files ${header_file})
  list(APPEND include_items "#include \"${header_name}\"")
  list(APPEND map_items "{\"${schema}\", ${schema_name}}")
endforeach()

# Adding the generated files to eat via `target_sources` does not add a
# dependency on the custom command, as eat is defined in a different directory
# to the command. Confusingly it also doesn't error despite the files not
# existing, as they are still flagged as being generated at build time. To work
# around this limitation, we add a custom target that depends on all the
# generated files, and add that as a dependency to eat. The custom target has ""
# as its command so is a no-op, it's just to make the dependency work.
# (otherwise the custom command would never run)
add_custom_target(
  generate_config_schema_includes
  ""
  DEPENDS ${header_files})
add_dependencies(eat generate_config_schema_includes)
# map_items and include_items used  to generate all_schemas.hpp
list(JOIN map_items ",\n" map_pairs)
list(JOIN include_items "\n" includes)
configure_file(all_schemas.in "${config_src_dir}/all_schemas.hpp" @ONLY)
target_include_directories(eat PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
