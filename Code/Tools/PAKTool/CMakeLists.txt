project(Xen2D)

add_executable(PAKTool main.cpp)

target_link_libraries(PAKTool PRIVATE
        Xen::Common
        Xen::PAK
        CLI11::CLI11
)

target_include_directories(PAKTool PRIVATE ${MODULES_ROOT} ${VENDOR_ROOT})