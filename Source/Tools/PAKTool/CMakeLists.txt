add_executable(PAKTool main.cpp)
# Same name the installed package exports (XenToolsTargets), so XenGame.cmake
# can refer to Xen::PAKTool either way.
add_executable(Xen::PAKTool ALIAS PAKTool)

set_target_properties(PAKTool PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/PAKTool")
foreach (config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config} config_upper)
    set_target_properties(PAKTool PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/PAKTool")
endforeach ()

target_link_libraries(PAKTool PRIVATE
        Xen::Common
        Xen::PAK
        CLI11::CLI11
)
