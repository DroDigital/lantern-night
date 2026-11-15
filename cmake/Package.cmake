# Platform packaging metadata. The archives themselves are produced by tools/package.cmake.
if(APPLE)
    set(LK_ICNS "${CMAKE_SOURCE_DIR}/packaging/macos/icon.icns")
    target_sources(LanternKnight PRIVATE ${LK_ICNS})
    set_source_files_properties(${LK_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(LanternKnight PROPERTIES
        MACOSX_BUNDLE ON
        OUTPUT_NAME "LanternKnight"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "Lantern Knight"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.lanternknight"
        MACOSX_BUNDLE_ICON_FILE "icon.icns"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${LK_VERSION_NUMERIC}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${LK_VERSION_NUMERIC}")
endif()
if(WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/packaging/windows/version.h.in ${CMAKE_BINARY_DIR}/lk_version_rc.h @ONLY)
    target_include_directories(LanternKnight PRIVATE ${CMAKE_BINARY_DIR})
endif()

# `cmake --build --preset release --target package_game` writes the archive for this OS into build/<preset>/dist/.
add_custom_target(package_game
    COMMAND ${CMAKE_COMMAND}
        -DLK_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DLK_BIN_DIR=${LK_BIN_DIR}
        -DLK_DIST_DIR=${CMAKE_BINARY_DIR}/dist
        -DLK_VERSION=${LK_VERSION}
        -P ${CMAKE_SOURCE_DIR}/tools/package.cmake
    DEPENDS LanternKnight
    COMMENT "Packaging Lantern Knight ${LK_VERSION}")
