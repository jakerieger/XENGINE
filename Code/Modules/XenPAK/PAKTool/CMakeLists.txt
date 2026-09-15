project(Xen2D)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Tools")

add_executable(PAKTool main.cpp)

target_link_libraries(PAKTool PRIVATE
        Xen::PAK
        CLI11::CLI11
)

target_include_directories(PAKTool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ ${MODULES_ROOT} ${VENDOR_ROOT})