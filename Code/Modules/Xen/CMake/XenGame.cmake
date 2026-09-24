include_guard(GLOBAL)

# CACHE INTERNAL, not a plain set(): include_guard(GLOBAL) means this file's
# body only ever runs once for the whole build, in whichever subdirectory
# scope happens to include() it first (previously always XenPong's, the only
# consumer). A plain variable set there is invisible to a sibling
# add_subdirectory() scope like XenPBRDemo's - only a cache variable is
# visible everywhere regardless of which scope first ran this file.
set(_XEN_GAME_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "")

set(XEN_DEFAULT_PAK "Data.pxk")

# Creates the game's executable target. Windows-only (WIN32 subsystem, so
# the game doesn't get a console window) - the engine dropped cross-platform
# support in the D3D12 migration, so there's no other subsystem to pick.
function(xen_add_game_executable TARGET)
    add_executable(${TARGET} WIN32 ${ARGN})

    # Each game gets its own output subdirectory. The top-level CMakeLists.txt
    # sets CMAKE_RUNTIME_OUTPUT_DIRECTORY_<CONFIG> once, project-wide, for
    # every target - on a multi-config generator (Visual Studio) that always
    # wins over a plain CMAKE_RUNTIME_OUTPUT_DIRECTORY reassignment in a
    # game's own CMakeLists.txt, so only the per-target RUNTIME_OUTPUT_
    # DIRECTORY(_<CONFIG>) properties actually override it. Without this,
    # every game executable in the project lands in the same flat bin/
    # folder and their POST_BUILD Config copies (and, in a shippable build,
    # their PAK_FILENAME) fight over the same files - invisible with one
    # game, a real collision the moment a second one exists.
    set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/Bin64")
    foreach (config ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${config} config_upper)
        set_target_properties(${TARGET} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/Bin64")
    endforeach ()
endfunction()

function(xen_configure_game TARGET)
    cmake_parse_arguments(ARG
            "ALLOW_NO_PAK;LOOSE_ASSETS_IN_RELEASE;NO_COMMANDLINE_CONTENT_DIRS"
            "PAK_FILENAME"
            "CONTENT_DIRS"
            ${ARGN})

    if (NOT ARG_PAK_FILENAME AND NOT ARG_ALLOW_NO_PAK)
        message(FATAL_ERROR
                "xen_configure_game(${TARGET}): PAK_FILENAME is required for shippable "
                "builds. Pass ALLOW_NO_PAK if this target is intentionally loose-only.")
    endif ()
    if (NOT ARG_CONTENT_DIRS)
        message(FATAL_ERROR "xen_configure_game(${TARGET}): CONTENT_DIRS is required for development builds.")
    endif ()

    # Recorded as a target property so xen_package_game_content can pick it
    # back up without every game having to repeat the same filename at both
    # call sites.
    if (ARG_PAK_FILENAME)
        set_target_properties(${TARGET} PROPERTIES XEN_PAK_FILENAME "${ARG_PAK_FILENAME}")
    endif ()

    # Quote as raw string literals so Windows paths survive intact.
    set(XEN_GEN_PAK_FILES "")
    if (ARG_PAK_FILENAME)
        set(XEN_GEN_PAK_FILES "R\"(${ARG_PAK_FILENAME})\"")
    endif ()

    set(XEN_ENGINE_SHADERS_PAK "R\"(EngineContent/XEN.Shaders.pxk)\"")

    set(XEN_ENGINE_ENVIRONMENT_PAK "R\"(EngineContent/XEN.Environment.pxk)\"")

    # Debug-only (see XenGameSettings.h.in's #ifdef NDEBUG split) - absolute
    # paths into the engine's OWN source tree, for ShaderHotReload. Baked in
    # unconditionally here; it's the generated header's #ifdef, not this
    # value, that keeps them out of a Release build.
    set(XEN_ENGINE_SHADER_SOURCE_DIR "R\"(${CMAKE_SOURCE_DIR}/Code/Shaders)\"")
    set(XEN_ENGINE_SHADER_OUTPUT_DIR "R\"(${CMAKE_SOURCE_DIR}/EngineContent/Shaders)\"")

    # dxc.exe isn't normally on a plain user/system PATH - only on the one a
    # Visual Studio dev-tools shell (vcvars) sets up, which is what this
    # whole build already runs under (compile_engine_shaders.py relies on
    # exactly that). The running GAME's own process almost certainly won't
    # have that PATH, so ShaderHotReload needs dxc.exe's absolute location
    # baked in rather than trusting its own ambient PATH at runtime -
    # resolved once here, in the same environment that's already proven to
    # find it (this configure step runs under whatever shell invoked CMake).
    find_program(XEN_DXC_EXECUTABLE dxc.exe)
    if (NOT XEN_DXC_EXECUTABLE)
        message(WARNING "xen_configure_game(${TARGET}): dxc.exe not found on PATH at configure time - "
                "ShaderHotReload will fall back to a bare 'dxc.exe' PATH lookup at runtime, which will "
                "likely fail unless the game is launched from a shell with the VS dev tools on PATH.")
        set(XEN_ENGINE_DXC_PATH "R\"()\"")
    else ()
        set(XEN_ENGINE_DXC_PATH "R\"(${XEN_DXC_EXECUTABLE})\"")
    endif ()

    set(XEN_GEN_CONTENT_DIRS "")
    foreach (dir IN LISTS ARG_CONTENT_DIRS)
        get_filename_component(dir "${dir}" ABSOLUTE)
        string(APPEND XEN_GEN_CONTENT_DIRS "R\"(${dir})\",")
    endforeach ()

    set(XEN_GEN_ALLOW_CLI_DIRS "true")
    if (ARG_NO_COMMANDLINE_CONTENT_DIRS)
        set(XEN_GEN_ALLOW_CLI_DIRS "false")
    endif ()

    set(XEN_GEN_LOOSE_IN_RELEASE "false")
    if (ARG_LOOSE_ASSETS_IN_RELEASE)
        set(XEN_GEN_LOOSE_IN_RELEASE "true")
    endif ()

    set(gen_dir "${CMAKE_CURRENT_BINARY_DIR}/XenGenerated/${TARGET}")
    configure_file(
            "${_XEN_GAME_CMAKE_DIR}/XenGameSettings.h.in"
            "${gen_dir}/Xen/XenGameSettings.h"
            @ONLY)

    target_include_directories(${TARGET} PRIVATE "${gen_dir}")
    target_link_libraries(${TARGET} PRIVATE Xen::Xen)
endfunction()

# Compiles the engine's shared HLSL (Code/Shaders/*.hlsl) to DXIL once per
# build, then makes TARGET depend on that output. Every game shares the same
# compiled Engine/Shaders output, so the underlying custom target is created
# only once, guarded by `if (NOT TARGET ...)`: without the guard, a second
# game calling this in the same CMake configure re-declares the same global
# target name and CMake hard-errors with "another target with the same name
# already exists".
function(xen_compile_shaders TARGET)
    if (NOT TARGET compile_engine_shaders)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        add_custom_target(compile_engine_shaders ALL
                COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/Scripts/compile_engine_shaders.py"
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                COMMENT "Compiling engine shaders..."
        )
    endif ()

    add_dependencies(${TARGET} compile_engine_shaders)
endfunction()

# Packages a configured game's runtime content into the distribution layout
# every game shares (see README.md's Game Distribution Output layout):
# Config/ copied loose next to the exe, the engine's shared shader/environment
# paks built from Code/Shaders and Engine/Environment, and the game's own
# CONTENT_DIR packed into PAK_FILENAME. This used to be five separate
# add_custom_command blocks pasted into every game's CMakeLists.txt - one
# call here instead of five copy-pasted ones means they can't drift out of
# sync (order, missing PAKTool dependency, etc.) between games.
#
# Depends on PAKTool directly, since this is the step that actually invokes
# PAKTool.exe as a POST_BUILD command - call this (or otherwise depend on
# PAKTool) before building a game, or its POST_BUILD pack steps will fail
# with "the system cannot find the path specified" against a PAKTool.exe that
# was never built.
#
# PAK_FILENAME can be omitted if xen_configure_game(TARGET PAK_FILENAME ...)
# was already called for this target - it reads back the XEN_PAK_FILENAME
# property that call recorded, so the filename isn't repeated at both call
# sites.
function(xen_package_game_content TARGET)
    cmake_parse_arguments(ARG
            ""
            "PAK_FILENAME;CONTENT_DIR;CONFIG_DIR"
            ""
            ${ARGN})

    if (NOT ARG_PAK_FILENAME)
        get_target_property(ARG_PAK_FILENAME ${TARGET} XEN_PAK_FILENAME)
    endif ()
    if (NOT ARG_PAK_FILENAME OR ARG_PAK_FILENAME STREQUAL "ARG_PAK_FILENAME-NOTFOUND")
        message(FATAL_ERROR
                "xen_package_game_content(${TARGET}): PAK_FILENAME is required, either "
                "passed here or via a prior xen_configure_game(${TARGET} PAK_FILENAME ...).")
    endif ()
    if (NOT ARG_CONTENT_DIR)
        set(ARG_CONTENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Content")
    endif ()
    if (NOT ARG_CONFIG_DIR)
        set(ARG_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Config")
    endif ()

    set(out_dir "$<TARGET_FILE_DIR:${TARGET}>/..")


    set(encrypt_flag "--encrypt=$<CONFIG:Release>")
    set(metadata_flag "--metadata=$<CONFIG:Debug>")

    add_custom_command(
            TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ARG_CONFIG_DIR}" "${out_dir}/Config"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${out_dir}/EngineContent"
            COMMAND "${TOOLS_BIN_DIR}/PAKTool.exe" pack "${CMAKE_SOURCE_DIR}/EngineContent/Shaders" -o "${out_dir}/EngineContent/XEN.Shaders.pxk" -i "${CMAKE_SOURCE_DIR}/.pakignore" ${encrypt_flag} ${metadata_flag}
            COMMAND "${TOOLS_BIN_DIR}/PAKTool.exe" pack "${CMAKE_SOURCE_DIR}/EngineContent/Environment" -o "${out_dir}/EngineContent/XEN.Environment.pxk" -i "${CMAKE_SOURCE_DIR}/.pakignore" ${encrypt_flag} ${metadata_flag}
            COMMAND "${TOOLS_BIN_DIR}/PAKTool.exe" pack "${ARG_CONTENT_DIR}" -o "${out_dir}/${ARG_PAK_FILENAME}" -i "${CMAKE_SOURCE_DIR}/.pakignore" ${encrypt_flag} ${metadata_flag}
            COMMENT "Packaging ${TARGET} content (Config, Engine shaders/environment, ${ARG_PAK_FILENAME})..."
            VERBATIM
    )

    add_dependencies(${TARGET} PAKTool)
endfunction()