message("Generating ${SCHEMA_HEADER}")
file(TOUCH "${SCHEMA_HEADER}")
file(READ "${SCHEMA}" schema_contents)
configure_file("${CMAKE_CURRENT_LIST_DIR}/schema.in" "${SCHEMA_HEADER}" @ONLY)
