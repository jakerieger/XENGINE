project(XENGINE)

add_executable(ResTool
        ${VENDOR_ROOT}/stb_image.cpp
        ${VENDOR_ROOT}/stb_image.h
        main.cpp
)

set_target_properties(ResTool PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ResTool")
foreach (config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config} config_upper)
    set_target_properties(ResTool PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ResTool")
endforeach ()

target_link_libraries(ResTool PRIVATE
        Xen::Common
        CLI11::CLI11
        brotlicommon
        brotlidec
        brotlienc
)
target_include_directories(ResTool PRIVATE ${MODULES_ROOT} ${VENDOR_ROOT})