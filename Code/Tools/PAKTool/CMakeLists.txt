project(Xen2D)

add_executable(PAKTool main.cpp)

# CMAKE_RUNTIME_OUTPUT_DIRECTORY here is already "<bin>/Tools" (set by the
# parent Tools/CMakeLists.txt), but that plain variable reassignment has no
# effect on a multi-config generator (Visual Studio) - only per-target
# RUNTIME_OUTPUT_DIRECTORY(_<CONFIG>) properties actually override the
# per-config global the top-level CMakeLists.txt sets. Same root cause as
# xen_add_game_executable's Bin64 fix, for the same reason: without this,
# PAKTool.exe silently landed in the flat bin/ root instead of bin/Tools/,
# breaking every caller that references it via TOOLS_BIN_DIR (Scripts/
# pack_demo_content.py, XenPBRDemo's engine-shader/environment pack steps).
set_target_properties(PAKTool PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
foreach (config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config} config_upper)
    set_target_properties(PAKTool PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endforeach ()

target_link_libraries(PAKTool PRIVATE
        Xen::Common
        Xen::PAK
        CLI11::CLI11
)

target_include_directories(PAKTool PRIVATE ${MODULES_ROOT} ${VENDOR_ROOT})