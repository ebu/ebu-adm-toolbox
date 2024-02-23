cmake_minimum_required(VERSION 3.21)
if(DEFINED PROJECT_NAME)
  set(EAT_IS_PRIMARY_PROJECT FALSE)
else()
  set(EAT_IS_PRIMARY_PROJECT TRUE)
endif()

project(eat VERSION 0.0.1)

# Options

# Probably only ever turn this off if doing a formatting check on CI
option(EAT_BUILD "enable build of ADM-Toolbox" ON)

option(EAT_BUILD_TESTS "enable build of ADM-Toolbox tests"
       ${EAT_IS_PRIMARY_PROJECT})

option(EAT_JUNIT_TEST_OUTPUT "output test results as junit xml" OFF)

option(EAT_BUILD_APPS "enable build of applications" ${EAT_IS_PRIMARY_PROJECT})

option(EAT_BUILD_EXAMPLES "enable build of examples" ${EAT_IS_PRIMARY_PROJECT})

option(EAT_APPLY_COMPILER_WARNINGS
       "apply a default set of warnings to eat targets"
       ${EAT_IS_PRIMARY_PROJECT})

option(EAT_INSTALL "deploy EAT via install target" ${EAT_IS_PRIMARY_PROJECT})

option(EAT_EXPORT_TARGETS "export and install EAT targets as cmake config"
       ${EAT_IS_PRIMARY_PROJECT})

# Targets not added to all and don't fail the build if dependencies are missing
# so OK to leave on by default, but handy to disable for reduced noise.
option(EAT_FORMAT
       "enable targets for formatting via clang-format and cmake-format"
       ${EAT_IS_PRIMARY_PROJECT})

option(EAT_DISABLE_CMAKE_FORMAT "do not use cmake-format during format targets"
       OFF)

# Consider enabling warnings as errors while working on project
option(EAT_WARNINGS_AS_ERRORS "treat warnings as errors for eat targets" OFF)

option(EAT_USE_FOLDERS "Organise IDE project into folders where supported"
       ${EAT_IS_PRIMARY_PROJECT})

if(EAT_BUILD)
  # Set cmake standard
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_EXTENSIONS OFF)
  if(EAT_USE_FOLDERS)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
  endif()

  # Get compile options
  list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
  include(eat_compile_options)
  get_eat_compile_options(VAR EAT_COMPILE_OPTIONS)

  # Add eat library target
  add_library(eat)
  add_library(EBU::eat ALIAS eat)

  target_include_directories(
    eat
    # we install to a default include path, so this is only needed if including
    # from build interface - i.e. when building converter tool or tests as part
    # of this project
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>
    PRIVATE src ${CMAKE_CURRENT_BINARY_DIR}/src)
  target_compile_options(eat PRIVATE ${EAT_COMPILE_OPTIONS})

  # Find dependencies (we should fix these to be quiet when asked)
  find_package(adm 0.13 QUIET REQUIRED CONFIG)
  find_package(bw64 0.10 QUIET REQUIRED CONFIG)
  find_package(ear 0.9 QUIET REQUIRED CONFIG)
  find_package(nlohmann_json 3.2.0 QUIET REQUIRED CONFIG)
  find_package(CLI11 1.9.0 QUIET REQUIRED CONFIG)
  find_package(unofficial-libebur128 QUIET REQUIRED CONFIG)
  find_package(valijson QUIET REQUIRED CONFIG)

  # Link dependencies
  target_link_libraries(eat PUBLIC adm bw64 ear)
  target_link_libraries(eat PRIVATE unofficial::ebur128
                                    nlohmann_json::nlohmann_json valijson)

  # Add eat test target
  if(EAT_BUILD_TESTS)
    find_package(Catch2 3.0 QUIET CONFIG REQUIRED)
    include(CTest)
    add_executable(test_eat)
    target_link_libraries(test_eat PRIVATE Catch2::Catch2WithMain EBU::eat)
    target_compile_definitions(
      test_eat PRIVATE "EAT_SRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}")

    include(Catch)

    if(EAT_JUNIT_TEST_OUTPUT)
      set(testOutputDir "${CMAKE_CURRENT_BINARY_DIR}/test_results")
      file(MAKE_DIRECTORY "${testOutputDir}")
      catch_discover_tests(
        test_eat
        REPORTER
        junit
        OUTPUT_DIR
        ${testOutputDir}
        OUTPUT_SUFFIX
        .xml)
    else()
      catch_discover_tests(test_eat)
    endif()

  endif()

  # Add sources for library and test targets (if configured)
  add_subdirectory(src/eat)
  add_subdirectory(include/eat)
  add_subdirectory(data/schemas)

  if(EAT_INSTALL)
    include(GNUInstallDirs)
    # Needed to fix docdir when included as subproject
    set(CMAKE_INSTALL_DOCDIR ${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_NAME})

    if((NOT CMAKE_INSTALL_PREFIX)
       AND (NOT WIN32)
       AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
      set(CMAKE_INSTALL_PREFIX "/opt/ebu.ch/eat")
    endif()

    # Install library and headers
    install(DIRECTORY include/ DESTINATION include)

    install(
      TARGETS eat
      EXPORT eat_exports
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    # Export targets as cmake config
    if(EAT_EXPORT_TARGETS)
      if(NOT BUILD_SHARED_LIBS)
        set(postfix _static)
      endif()

      install(
        EXPORT eat_exports
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/eat
        FILE eat${postfix}.cmake
        NAMESPACE EBU::)

      configure_file(eatConfig.cmake.in eatConfig.cmake @ONLY)

      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/eatConfig.cmake
              DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/eat)

      export(
        EXPORT eat_exports
        NAMESPACE EBU::
        FILE eat${postfix}.cmake)
    endif()
  endif()

  if(EAT_BUILD_APPS)
    add_executable(eat-process)
    target_link_libraries(eat-process PRIVATE EBU::eat CLI11::CLI11)

    add_subdirectory(src/apps)

    if(EAT_INSTALL)
      install(TARGETS eat-process RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
  endif()

  if(EAT_BUILD_EXAMPLES)
    add_executable(eat-measure-loudness)
    target_link_libraries(eat-measure-loudness PRIVATE EBU::eat)

    add_subdirectory(src/examples)
  endif()
endif()

# Add formatting targets
if(EAT_FORMAT)
  if(CMAKE_FOLDER)
    set(old_folder $CMAKE_FOLDER)
  endif()
  set(CMAKE_FOLDER "formatting")
  include(cmake/CPM.cmake)
  cpmaddpackage(
    NAME
    Format.cmake
    VERSION
    1.7.3
    GITHUB_REPOSITORY
    TheLartians/Format.cmake
    OPTIONS
    # set to yes skip cmake formatting
    "FORMAT_SKIP_CMAKE ${EAT_DISABLE_CMAKE_FORMAT}"
    # path to exclude (optional, supports regular expressions)
    "CMAKE_FORMAT_EXCLUDE (cmake/CPM.cmake|external.*)")
  if(old_folder)
    set(CMAKE_FOLDER ${old_folder})
  else()
    unset(CMAKE_FOLDER)
  endif()
endif()
