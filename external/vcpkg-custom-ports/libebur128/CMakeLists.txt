cmake_minimum_required(VERSION 3.14)
project(libebur128_unofficial LANGUAGES C)

add_library(ebur128)

if(MSVC)
  target_sources(ebur128 PRIVATE ebur128/ebur128.c ebur128/ebur128.def)
else()
  target_sources(ebur128 PRIVATE ebur128/ebur128.c)
endif()

target_include_directories(
  ebur128
  PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDE_DIR}>)

set_target_properties(ebur128 PROPERTIES PUBLIC_HEADER ebur128/ebur128.h)

install(TARGETS ebur128 EXPORT unofficial-libebur128-config)

install(
    EXPORT unofficial-libebur128-config
    NAMESPACE unofficial::
    DESTINATION share/unofficial-libebur128
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
