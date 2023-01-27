# Returns a list of compiler flags in variable supplied as VAR argument.
#
# These will include:
#
# * A default set of compiler dependent warnings if EAT_APPLY_COMPILER_WARNINGS
#   is True.
# * A warning as errors flag if EAT_WARNINGS_AS_ERRORS is true.
function(get_eat_compile_options)
  set(options "")
  set(oneValueArgs VAR)
  set(multiValueArgs "")
  cmake_parse_arguments(OPT "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  # preserve passed in options so can be augmented externally
  if(DEFINED ${OPT_VAR})
    set(chosen_options ${OPT_VAR})
  endif()

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(GCC TRUE)
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CLANG TRUE)
  endif()

  if(GCC OR CLANG)
    set(GCC_LIKE TRUE)
  endif()

  if(GCC_LIKE)
    if(EAT_APPLY_COMPILER_WARNINGS)
      list(
        APPEND
        chosen_options
        -pedantic
        -Wall
        -Wextra
        -Wcast-align
        -Wcast-qual
        -Wformat=2
        -Wmissing-declarations
        -Wmissing-include-dirs
        -Wold-style-cast
        -Woverloaded-virtual
        -Wredundant-decls
        -Wshadow
        -Wsign-conversion
        -Wsign-promo
        -Wstrict-overflow=4
        -Wswitch-default
        -Wundef
        -Wzero-as-null-pointer-constant)
    endif()
    if(EAT_WARNINGS_AS_ERRORS)
      list(APPEND chosen_options -Werror)
    endif()
  endif()

  # these aren't recognised by apple clang, should check vanilla clang
  if(GCC AND EAT_APPLY_COMPILER_WARNINGS)
    list(APPEND chosen_options -Wlogical-op -Wnoexcept -Wstrict-null-sentinel
         -Wuseless-cast)
  endif()

  if(MSVC)
    list(APPEND chosen_options /bigobj)
    if(EAT_APPLY_COMPILER_WARNINGS)
      # might need to tune this to better match gcc
      list(APPEND chosen_options /Wall)
    endif()
    if(EAT_WARNINGS_AS_ERRORS)
      list(APPEND chosen_options /WX)
    endif()
  endif()

  message(DEBUG "eat_options.cmake has selected the following compile flags:")
  foreach(opt ${chosen_options})
    message(DEBUG "\t${opt}")
  endforeach()

  set(${OPT_VAR}
      ${chosen_options}
      PARENT_SCOPE)
endfunction()
