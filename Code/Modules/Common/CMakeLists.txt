project(Xen2D)

add_library(XenCommon STATIC
        DateTime.hpp
        Exception.hpp
        Log.cpp
        Log.hpp
        Signature.hpp
        XenCommon.hpp
)

add_library(Xen::Common ALIAS XenCommon)
