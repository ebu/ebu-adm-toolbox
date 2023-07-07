vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
        FEATURES sadm FEATURE_SADM)

if(FEATURE_SADM)
    # SADM-v2 branch
    set(git_ref 792d050f672a83b11b1091eda5e3b4385c4e3bbb)
    set(hash 65bbe1d4021b088c3fd725e501928b9d626729fdcfd2f80ee813c58b0e1a0935c63727ffac354fce10a05891f45bf214d2e3efe961ea95ed612680a97f907d5d)
else()
    # main branch
    set(git_ref c9c168b3f349625734ba836f3d6f9794d7e5560e)
    set(hash d511809896b76fc2e22d27803f6473d3aa07c7f3f0d156f1856af0213c557177f8af5361d50521d603f5f65ba533c97dc6266231637b554e7259d69a0d2b9852)
endif()
vcpkg_from_github(
  OUT_SOURCE_PATH
  SOURCE_PATH
  REPO
  ebu/libadm
  REF ${git_ref}
  SHA512 ${hash}
  HEAD_REF
  draft)

vcpkg_cmake_configure(SOURCE_PATH ${SOURCE_PATH} OPTIONS -DADM_UNIT_TESTS=OFF
                      -DADM_EXAMPLES=OFF)
vcpkg_cmake_install()

if(WIN32 AND NOT CYGWIN)
    set(config_dir CMake)
else()
    set(config_dir share/cmake/adm)
endif()

vcpkg_cmake_config_fixup(PACKAGE_NAME adm
        CONFIG_PATH ${config_dir})
file(
  INSTALL "${SOURCE_PATH}/LICENSE"
  DESTINATION "${CURRENT_PACKAGES_DIR}/share/adm"
  RENAME copyright)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
