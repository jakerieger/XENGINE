# Install rules and the find_package(Xen) package config.
#
# Debug and Release are meant to be installed into the same prefix (the
# libraries carry a 'd' Debug postfix - see the top-level CMakeLists.txt):
#
#   cmake --install build/Debug   --prefix <prefix>
#   cmake --install build/Release --prefix <prefix>
#
# A game then only needs -DCMAKE_PREFIX_PATH=<prefix> and find_package(Xen).
#
# Layout:
#   bin/                    tools (Release builds only)
#   include/                Xen/ Common/ XenPAK/ (public engine headers), D3D12MemAlloc.h, directx/
#   include/XenVendor/      vendored headers games use: imgui, nlohmann/json, ini.h
#   lib/                    engine + dependency static libs (Debug ones end in 'd')
#   share/Xen/cmake/        XenConfig.cmake and friends
#   share/directx-headers/, share/cmake/D3D12MemoryAllocator/ - those deps' own packages

include(CMakePackageConfigHelpers)

set(XEN_INSTALL_CMAKEDIR "${CMAKE_INSTALL_DATAROOTDIR}/Xen/cmake")

set_target_properties(XenCommon PROPERTIES EXPORT_NAME Common)
set_target_properties(XenPAK PROPERTIES EXPORT_NAME PAK)

# lz4_static is linked PRIVATE by XenPAK, but a static library's private link
# dependencies still have to be linked by whatever finally links it, so the
# export must carry it too. lz4 runs in bundled mode (no install rules of its
# own), so it rides along in our export set as Xen::lz4_static.
install(TARGETS XenCommon XenPAK Xen lz4_static
        EXPORT XenTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILE_SET vendor DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/XenVendor
        FILE_SET imgui DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/XenVendor
)
install(EXPORT XenTargets
        NAMESPACE Xen::
        DESTINATION ${XEN_INSTALL_CMAKEDIR}
)

# Tools are installed from Release builds only - otherwise the Debug and
# Release installs would overwrite each other's bin/*.exe. PAKTool is exported
# (as Xen::PAKTool) because game content packaging runs it.
if (TARGET PAKTool)
    install(TARGETS PAKTool
            EXPORT XenToolsTargets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} CONFIGURATIONS Release
    )
    install(EXPORT XenToolsTargets
            NAMESPACE Xen::
            DESTINATION ${XEN_INSTALL_CMAKEDIR}
            CONFIGURATIONS Release
    )
endif ()
if (TARGET Bin2CC)
    install(TARGETS Bin2CC RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} CONFIGURATIONS Release)
endif ()

configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/XenConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/XenConfig.cmake
        INSTALL_DESTINATION ${XEN_INSTALL_CMAKEDIR}
)
# Pre-1.0, a minor version bump may break the API, so only an exact
# major.minor match is compatible.
write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/XenConfigVersion.cmake
        COMPATIBILITY SameMinorVersion
)
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/XenConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/XenConfigVersion.cmake
        DESTINATION ${XEN_INSTALL_CMAKEDIR}
)
