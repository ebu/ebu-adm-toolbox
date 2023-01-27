# Manual clone as need submodules
find_program(GIT git)

set(GIT_URL "https://github.com/ebu/libear.git")
set(GIT_REV "b2c3be5b9c6b4920aa91c11fedaa26d1c5229fcc")

set(SOURCE_PATH ${CURRENT_BUILDTREES_DIR}/src/${PORT})

if(NOT EXISTS "${SOURCE_PATH}/.git")
  message(STATUS "Cloning and fetching submodules")
  vcpkg_execute_required_process(
    COMMAND
    ${GIT}
    clone
    --recurse-submodules
    ${GIT_URL}
    ${SOURCE_PATH}
    WORKING_DIRECTORY
    ${CURRENT_BUILDTREES_DIR}
    LOGNAME
    clone)

  message(STATUS "Checkout revision ${GIT_REV}")
  vcpkg_execute_required_process(
    COMMAND
    ${GIT}
    checkout
    ${GIT_REV}
    WORKING_DIRECTORY
    ${SOURCE_PATH}
    LOGNAME
    checkout)
endif()

vcpkg_cmake_configure(SOURCE_PATH ${SOURCE_PATH} OPTIONS -DEAR_UNIT_TESTS=OFF
                      -DEAR_EXAMPLES=OFF)

vcpkg_cmake_install()

if(WIN32 AND NOT CYGWIN)
  set(config_dir CMake)
else()
  set(config_dir share/cmake/ear)
endif()

vcpkg_cmake_config_fixup(PACKAGE_NAME ear
        CONFIG_PATH ${config_dir})
file(
  INSTALL "${SOURCE_PATH}/LICENSE"
  DESTINATION "${CURRENT_PACKAGES_DIR}/share/ear"
  RENAME copyright)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
