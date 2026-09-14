project(Xen2D)

add_executable(PAKTool main.cpp)
target_link_libraries(PAKTool PRIVATE XenPAK)
target_include_directories(PAKTool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ ${MODULES_ROOT} ${VENDOR_ROOT})