vcpkg_from_github(
  OUT_SOURCE_PATH
  SOURCE_PATH
  REPO
  ebu/libbw64
  REF
  5ff824ff4f7d93bef2accb0125ac5a3e3d9247ed
  SHA512
  8c8b62b13869629da855bdf306607bba9a965cd4faf2ac86208c90ad9c1931015f393395655bd62fc80a07e12d2f63ab71e9d339597d318ab52e3a8e8f7090b1
  HEAD_REF
  master)

vcpkg_cmake_configure(SOURCE_PATH ${SOURCE_PATH} OPTIONS -DBW64_UNIT_TESTS=OFF
                      -DBW64_EXAMPLES=OFF)
vcpkg_cmake_install()

if(WIN32 AND NOT CYGWIN)
    set(config_dir CMake)
else()
    set(config_dir share/cmake/bw64)
endif()

vcpkg_cmake_config_fixup(PACKAGE_NAME bw64
        CONFIG_PATH ${config_dir})

file(INSTALL "${SOURCE_PATH}/LICENSE"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/bw64"
     RENAME copyright)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")
