project(Xen2D)

add_library(XenCommon STATIC
        DateTime.hpp
        Exception.hpp
        Hash.hpp
        Log.cpp
        Log.hpp
        Math.hpp
        Platform.hpp
        PrettySignature.hpp
        XenCommon.hpp
)

add_library(Xen::Common ALIAS XenCommon)
