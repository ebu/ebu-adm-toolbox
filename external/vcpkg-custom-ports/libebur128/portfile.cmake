if((VCPKG_TARGET_ARCHITECTURE STREQUAL "arm" OR VCPKG_TARGET_ARCHITECTURE STREQUAL "arm64") AND VCPKG_TARGET_IS_WINDOWS)
    message(FATAL_ERROR "${PORT} does not support Windows ARM")
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    PATCHES 0001-always-use-vendored-queue.h.patch 0002-don-t-use-non-standard-M_PI-macro.patch
    REPO jiixyj/libebur128
    REF v1.2.6
    SHA512 ab188c6d32cd14613119258313a8a3fb1167b55501c9f5b6d3ba738d674bc58f24ac3034c23d9730ed8dc3e95a23619bfb81719e4c79807a9a16c1a5b3423582
)

file(COPY ${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt DESTINATION ${SOURCE_PATH})

vcpkg_configure_cmake(
        SOURCE_PATH ${SOURCE_PATH}
        PREFER_NINJA
)

vcpkg_install_cmake()

vcpkg_fixup_cmake_targets(CONFIG_PATH share/unofficial-${PORT} TARGET_PATH share/unofficial-${PORT})

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL ${SOURCE_PATH}/COPYING DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
