#header-only library
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tristanpenman/valijson
    REF v1.0
    SHA512 a206954b11e92cbebbebf094e6f0925a270ebd6bec49cbdb7adda5a4cec93587a5a61ebbce105846c3950cf5df74bfdd5f5bb1ffbf73315f45c7a6cda2b77db9
    HEAD_REF master
)

vcpkg_cmake_configure(SOURCE_PATH ${SOURCE_PATH})
vcpkg_cmake_install()

vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/valijson)

# Put the licence file where vcpkg expects it
file(COPY ${SOURCE_PATH}/LICENSE
     DESTINATION ${CURRENT_PACKAGES_DIR}/share/valijson)
file(RENAME ${CURRENT_PACKAGES_DIR}/share/valijson/LICENSE ${CURRENT_PACKAGES_DIR}/share/valijson/copyright)
