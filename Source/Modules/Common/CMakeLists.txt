project(XENGINE)

add_library(XenCommon STATIC
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