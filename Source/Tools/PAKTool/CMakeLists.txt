project(XENGINE)

add_executable(PAKTool main.cpp)

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

target_include_directories(PAKTool PRIVATE ${MODULES_ROOT} ${VENDOR_ROOT})