include_guard(GLOBAL)

# Game-side CMake helpers. Works the same in-tree (the engine's own Sandbox
# and XED) and out-of-tree (a game using find_package(Xen), whose
# XenConfig.cmake puts this file on CMAKE_MODULE_PATH). Everything that
# differs between the two comes from these, defined by the engine's top-level
# CMakeLists.txt or by XenConfig.cmake respectively:
#
#   Xen::PAKTool                  the PAKTool executable target
#   XEN_ENGINE_PAK_DIR            directory holding the engine paks (XEN.*.pxk)
#                                 for the current config; may contain $<CONFIG>
#   XEN_PAKIGNORE                 the engine's default .pakignore
#   XEN_ENGINE_SHADER_SOURCE_DIR  (in-tree only) engine shader hot-reload
#   XEN_ENGINE_SHADER_OUTPUT_DIR    paths - unset means hot reload is off
#
# Nothing in here may reference CMAKE_SOURCE_DIR as "the engine": out of tree,
# that's the game's own project.

# CACHE INTERNAL, not a plain set(): include_guard(GLOBAL) means this file's
# body only ever runs once for the whole build, in whichever subdirectory
# scope happens to include() it first (previously always XenPong's, the only
# consumer). A plain variable set there is invisible to a sibling
# add_subdirectory() scope like XenPBRDemo's - only a cache variable is
# visible everywhere regardless of which scope first ran this file.
set(_XEN_GAME_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "")

set(XEN_DEFAULT_PAK "Data.pxk")

# Fails early, with a pointer at the fix, instead of producing a build whose
# POST_BUILD steps break with path errors.
function(_xen_require_engine_vars CALLER)
    foreach (var XEN_ENGINE_PAK_DIR XEN_PAKIGNORE)
        if (NOT DEFINED ${var})
            message(FATAL_ERROR "${CALLER}: ${var} is not set - include XenGame via find_package(Xen) "
                    "(or from within the engine's own build), not on its own.")
        endif ()
    endforeach ()
    if (NOT TARGET Xen::PAKTool)
        message(FATAL_ERROR "${CALLER}: Xen::PAKTool is not available. For an installed engine, install a "
                "Release build of it too - tools are only installed from Release builds.")
    endif ()
endfunction()

# Packaging runs as POST_BUILD steps, i.e. only when the game relinks - so
# editing only content or config would leave the packaged output stale. Making
# those files link dependencies of the executable fixes that: changing one
# relinks the game, which re-runs its packaging. CONFIGURE_DEPENDS picks up
# added/removed files too. (LINK_DEPENDS is honored by the Ninja and Makefile
# generators; Visual Studio ignores it, so there a rebuild is still needed.)
function(_xen_relink_on_changes TARGET DIR)
    if (IS_DIRECTORY "${DIR}")
        file(GLOB_RECURSE files CONFIGURE_DEPENDS "${DIR}/*")
        set_property(TARGET ${TARGET} APPEND PROPERTY LINK_DEPENDS ${files})
    endif ()
endfunction()

# Makes TARGET build after PAKTool when PAKTool is built in this same project
# (in-tree). An installed Xen::PAKTool is an IMPORTED target with nothing to build.
function(_xen_depend_on_paktool TARGET)
    get_target_property(paktool Xen::PAKTool ALIASED_TARGET)
    if (paktool)
        add_dependencies(${TARGET} ${paktool})
    endif ()
endfunction()

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
    #
    # A game project that doesn't set CMAKE_RUNTIME_OUTPUT_DIRECTORY itself
    # gets <build>/bin/<Game>/Bin64, matching the engine's own layout.
    set(base_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    if (NOT base_dir)
        set(base_dir "${CMAKE_BINARY_DIR}/bin")
    endif ()
    set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${base_dir}/${TARGET}/Bin64")
    foreach (config ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${config} config_upper)
        set_target_properties(${TARGET} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${base_dir}/${TARGET}/Bin64")
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

    set(_xen_pak_files)
    if (NOT "${XEN_GEN_PAK_FILES}" STREQUAL "")
        list(APPEND _xen_pak_files "${XEN_GEN_PAK_FILES}")
    endif ()
    list(APPEND _xen_pak_files
            "${XEN_ENGINE_SHADERS_PAK}"
            "${XEN_ENGINE_ENVIRONMENT_PAK}")

    list(JOIN _xen_pak_files ",\n                                         " XEN_PAK_FILES_LIST)

    # Debug-only (see XenGameSettings.h.in's #ifdef NDEBUG split) - absolute
    # paths into the engine's OWN source tree, for ShaderHotReload. Baked in
    # unconditionally here; it's the generated header's #ifdef, not this
    # value, that keeps them out of a Release build. Only an in-tree build
    # defines them - against an installed engine there's no shader source to
    # watch, and empty paths make ShaderHotReload::Initialize a quiet no-op.
    set(hot_reload_source_dir "${XEN_ENGINE_SHADER_SOURCE_DIR}")
    set(XEN_ENGINE_SHADER_SOURCE_DIR "R\"(${XEN_ENGINE_SHADER_SOURCE_DIR})\"")
    set(XEN_ENGINE_SHADER_OUTPUT_DIR "R\"(${XEN_ENGINE_SHADER_OUTPUT_DIR})\"")
    set(XEN_ENGINE_DXC_PATH "R\"()\"")

    # dxc.exe isn't normally on a plain user/system PATH - only on the one a
    # Visual Studio dev-tools shell (vcvars) sets up, which is what this
    # whole build already runs under (compile_engine_shaders.py relies on
    # exactly that). The running GAME's own process almost certainly won't
    # have that PATH, so ShaderHotReload needs dxc.exe's absolute location
    # baked in rather than trusting its own ambient PATH at runtime -
    # resolved once here, in the same environment that's already proven to
    # find it (this configure step runs under whatever shell invoked CMake).
    if (hot_reload_source_dir)
        find_program(XEN_DXC_EXECUTABLE dxc.exe)
        if (NOT XEN_DXC_EXECUTABLE)
            message(WARNING "xen_configure_game(${TARGET}): dxc.exe not found on PATH at configure time - "
                    "ShaderHotReload will fall back to a bare 'dxc.exe' PATH lookup at runtime, which will "
                    "likely fail unless the game is launched from a shell with the VS dev tools on PATH.")
        else ()
            set(XEN_ENGINE_DXC_PATH "R\"(${XEN_DXC_EXECUTABLE})\"")
        endif ()
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

# Makes TARGET build after the engine's shaders are compiled. Only meaningful
# in-tree, where the engine (and so its shaders) is part of the same build;
# an installed engine ships its shaders precompiled, so this is a no-op there.
function(xen_compile_shaders TARGET)
    if (TARGET compile_engine_shaders)
        add_dependencies(${TARGET} compile_engine_shaders)
    endif ()
endfunction()

# Copies the game's Config/ and the engine's shared paks (XEN.Shaders.pxk,
# XEN.Environment.pxk) next to the game's Bin64/ - see README.md's Game
# Distribution Output layout. The paks themselves are built once by the
# engine (CMake/EngineContent.cmake), not per game, and are only copied when
# they actually changed.
function(xen_package_engine_content TARGET)
    _xen_require_engine_vars(xen_package_engine_content)

    set(out_dir "$<TARGET_FILE_DIR:${TARGET}>/..")

    add_custom_command(
            TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Config" "${out_dir}/Config"
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${XEN_ENGINE_PAK_DIR}" "${out_dir}/EngineContent"
            COMMENT "Packaging ${TARGET} engine content..."
            VERBATIM
    )

    if (TARGET xen_engine_paks)
        add_dependencies(${TARGET} xen_engine_paks)
    endif ()
    _xen_relink_on_changes(${TARGET} "${CMAKE_CURRENT_SOURCE_DIR}/Config")
endfunction()

# Packs the game's own CONTENT_DIR into PAK_FILENAME next to its Bin64/.
# Encrypted in Release, with a .pxkm metadata file in Debug - same as the
# engine's paks.
#
# PAK_FILENAME can be omitted if xen_configure_game(TARGET PAK_FILENAME ...)
# was already called for this target - it reads back the XEN_PAK_FILENAME
# property that call recorded, so the filename isn't repeated at both call
# sites. IGNORE_FILE defaults to the project's own .pakignore if it has one,
# else the engine's.
function(xen_package_game_content TARGET)
    cmake_parse_arguments(ARG
            ""
            "PAK_FILENAME;CONTENT_DIR;IGNORE_FILE"
            ""
            ${ARGN})

    _xen_require_engine_vars(xen_package_game_content)

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
    if (NOT ARG_IGNORE_FILE)
        if (EXISTS "${CMAKE_SOURCE_DIR}/.pakignore")
            set(ARG_IGNORE_FILE "${CMAKE_SOURCE_DIR}/.pakignore")
        else ()
            set(ARG_IGNORE_FILE "${XEN_PAKIGNORE}")
        endif ()
    endif ()

    set(out_dir "$<TARGET_FILE_DIR:${TARGET}>/..")

    set(encrypt_flag "--encrypt=$<CONFIG:Release>")
    set(metadata_flag "--metadata=$<CONFIG:Debug>")

    add_custom_command(
            TARGET ${TARGET} POST_BUILD
            COMMAND "$<TARGET_FILE:Xen::PAKTool>" pack "${ARG_CONTENT_DIR}" -o "${out_dir}/${ARG_PAK_FILENAME}" -i "${ARG_IGNORE_FILE}" ${encrypt_flag} ${metadata_flag}
            COMMENT "Packaging ${TARGET} content (${ARG_PAK_FILENAME})..."
            VERBATIM
    )

    _xen_depend_on_paktool(${TARGET})
    _xen_relink_on_changes(${TARGET} "${ARG_CONTENT_DIR}")
    set_property(TARGET ${TARGET} APPEND PROPERTY LINK_DEPENDS "${ARG_IGNORE_FILE}")
endfunction()
