project(XENGINE)

add_library(XenCommon STATIC
        Brotli.cpp
        Brotli.hpp
        DateTime.hpp
        Exception.hpp
        Hash.hpp
        Ini.hpp
        Log.cpp
        Log.hpp
        Math.hpp
        Platform.hpp
        PrettySignature.hpp
        XenCommon.hpp
)

add_library(Xen::Common ALIAS XenCommon)
target_include_directories(XenCommon PRIVATE ${VENDOR_ROOT})
target_link_libraries(XenCommon PRIVATE brotlicommon brotlienc brotlidec)