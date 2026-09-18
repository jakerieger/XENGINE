include_guard(GLOBAL)

# CACHE INTERNAL, not a plain set(): include_guard(GLOBAL) means this file's
# body only ever runs once for the whole build, in whichever subdirectory
# scope happens to include() it first (previously always XenPong's, the only
# consumer). A plain variable set there is invisible to a sibling
# add_subdirectory() scope like XenPBRDemo's - only a cache variable is
# visible everywhere regardless of which scope first ran this file.
set(_XEN_GAME_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "")

# Creates the game's executable target, picking the right per-platform
# entry point subsystem (e.g. WIN32 on Windows so the game doesn't get a
# console window) so individual game CMakeLists don't have to branch on
# PLATFORM_WINDOWS/MACOS/LINUX themselves.
function(xen_add_game_executable TARGET)
    if (PLATFORM_WINDOWS)
        add_executable(${TARGET} WIN32 ${ARGN})
    else ()
        add_executable(${TARGET} ${ARGN})
    endif ()

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
                RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}")
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

    # Quote as raw string literals so Windows paths survive intact.
    set(XEN_GEN_PAK_FILES "")
    if (ARG_PAK_FILENAME)
        set(XEN_GEN_PAK_FILES "R\"(${ARG_PAK_FILENAME})\"")
    endif ()

    set(XEN_ENGINE_SHADERS_PAK "R\"(Engine/XEN.Shaders.xpak)\"")

    set(XEN_ENGINE_ENVIRONMENT_PAK "R\"(Engine/XEN.Environment.xpak)\"")

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