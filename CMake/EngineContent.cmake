# Builds the engine's shared runtime content once per engine build - not per
# game - into ${CMAKE_BINARY_DIR}/EngineContent/<Config>/ (XEN_ENGINE_PAK_DIR):
#
#   XEN.Shaders.pxk      Source/Shaders/*.hlsl compiled to DXIL, then packed
#   XEN.Environment.pxk  EngineContent/Environment (IBL etc.)
#
# plus .pxkm metadata in Debug. Games only copy these next to their executable
# (xen_package_engine_content), and an installed engine ships them prebuilt -
# so a game built against an install needs neither Python, dxc nor the raw
# engine content.
#
# Pak flags follow the engine build config: encrypted in Release, metadata in
# Debug. The key is built into XenPAK, so the paks are identical for every game.
#
# Must be included before any game (Sandbox, XED) is added: XenGame's helpers
# only add the build-order dependency on these targets if they already exist.
# PAKTool itself is only referenced by name here, so it can be defined later.

if (NOT BUILD_TOOLS)
    message(WARNING "PAKTool isn't being built (BUILD_TOOLS=OFF) - engine paks won't be built or installed, "
            "so games in this build can't be packaged.")
    return()
endif ()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Always runs: the script decides what needs recompiling. Its output lands in
# EngineContent/Shaders, which is also what ShaderHotReload watches in Debug.
add_custom_target(compile_engine_shaders ALL
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/Scripts/compile_engine_shaders.py"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Compiling engine shaders..."
)

set(_xen_encrypt_flag "--encrypt=$<CONFIG:Release>")
set(_xen_metadata_flag "--metadata=$<CONFIG:Debug>")

# Environment content is static, so it's a real build output: repacked only
# when a file under EngineContent/Environment (or the ignore list) changes.
file(GLOB_RECURSE _xen_environment_files CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/EngineContent/Environment/*")
add_custom_command(
        OUTPUT "${XEN_ENGINE_PAK_DIR}/XEN.Environment.pxk"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${XEN_ENGINE_PAK_DIR}"
        COMMAND PAKTool pack "${CMAKE_SOURCE_DIR}/EngineContent/Environment"
                -o "${XEN_ENGINE_PAK_DIR}/XEN.Environment.pxk"
                -i "${XEN_PAKIGNORE}" ${_xen_encrypt_flag} ${_xen_metadata_flag}
        DEPENDS PAKTool ${_xen_environment_files} "${XEN_PAKIGNORE}"
        COMMENT "Packing engine environment content..."
        VERBATIM
)

# The compiled shaders are produced by compile_engine_shaders during the same
# build, which the build tool can't see as file outputs - so the (small)
# shader pak is simply repacked on every build, after the compile step.
add_custom_target(xen_engine_paks ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${XEN_ENGINE_PAK_DIR}"
        COMMAND PAKTool pack "${CMAKE_SOURCE_DIR}/EngineContent/Shaders"
                -o "${XEN_ENGINE_PAK_DIR}/XEN.Shaders.pxk"
                -i "${XEN_PAKIGNORE}" ${_xen_encrypt_flag} ${_xen_metadata_flag}
        DEPENDS "${XEN_ENGINE_PAK_DIR}/XEN.Environment.pxk"
        COMMENT "Packing engine shaders..."
        VERBATIM
)
add_dependencies(xen_engine_paks compile_engine_shaders PAKTool)

unset(_xen_encrypt_flag)
unset(_xen_metadata_flag)
unset(_xen_environment_files)
