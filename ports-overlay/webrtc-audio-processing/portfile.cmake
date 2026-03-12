vcpkg_download_distfile(ARCHIVE
    URLS https://gitlab.freedesktop.org/pulseaudio/webrtc-audio-processing/-/archive/v1.3/webrtc-audio-processing-v1.3.tar.gz
    FILENAME webrtc-audio-processing-v1.3.tar.gz
    SHA512 addd6feb5f46f958786b0befadbafe1247737a5cd002a631d9fb2c85fd121959287079148a73f3a20f2b594b288cd5697ab28f4063a4d421fbee9ed2f1ea2117
)

vcpkg_extract_source_archive_ex(
    OUT_SOURCE_PATH SOURCE_PATH
    ARCHIVE ${ARCHIVE}
)

file(READ ${SOURCE_PATH}/meson.build MESON_IN)
string(REPLACE "meson_version : '>= 0.63'" "meson_version : '>= 0.58'" MESON_OUT "${MESON_IN}")
file(WRITE ${SOURCE_PATH}/meson.build "${MESON_OUT}")

vcpkg_configure_meson(
    SOURCE_PATH ${SOURCE_PATH}
    OPTIONS -Dtests=disabled
)

# Meson mis-detects the MSVC librarian and emits ar-style flags ("csr") which break lib.exe.
# Patch the generated build.ninja files to use proper /OUT syntax.
foreach(config dbg rel)
    set(ninja_file "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-${config}/build.ninja")
    if (EXISTS "${ninja_file}")
        file(READ "${ninja_file}" _ninja_contents)
        string(REPLACE "LINK_ARGS = \"csr\"" "LINK_ARGS = \"\"" _ninja_contents "${_ninja_contents}")
        string(REPLACE "\$LINK_ARGS \$out \$in" "/OUT:\$out \$in" _ninja_contents "${_ninja_contents}")
        file(WRITE "${ninja_file}" "${_ninja_contents}")
    endif()
endforeach()

vcpkg_install_meson()
vcpkg_fixup_pkgconfig()

include(CMakePackageConfigHelpers)

set(config_dir "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(MAKE_DIRECTORY "${config_dir}")

configure_file("${CMAKE_CURRENT_LIST_DIR}/webrtc-audio-processing-config.cmake.in"
               "${config_dir}/webrtc-audio-processing-config.cmake"
               @ONLY)
write_basic_package_version_file(
    "${config_dir}/webrtc-audio-processing-config-version.cmake"
    VERSION "1.3"
    COMPATIBILITY SameMajorVersion
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")

file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/share)
vcpkg_copy_pdbs()
