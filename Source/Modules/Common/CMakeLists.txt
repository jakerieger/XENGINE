project(XENGINE)

add_library(XenCommon STATIC
        Brotli.cpp
        Brotli.hpp
        DateTime.hpp
        Exception.hpp
        Hash.hpp
        Ini.hpp
        Io.cpp
        Io.hpp
        Log.cpp
        Log.hpp
        Math.hpp
        Platform.hpp
        PrettySignature.hpp
        XenCommon.hpp
)

target_compile_definitions(XenCommon PUBLIC
        XEN_ENGINE_VERSION="dev"
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)

add_library(Xen::Common ALIAS XenCommon)
target_include_directories(XenCommon PRIVATE ${VENDOR_ROOT})
target_link_libraries(XenCommon PRIVATE brotlicommon brotlienc brotlidec)