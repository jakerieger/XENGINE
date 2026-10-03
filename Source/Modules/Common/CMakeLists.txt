add_library(XenCommon STATIC
        Io.cpp
        Log.cpp
)
add_library(Xen::Common ALIAS XenCommon)

# Public headers, included as <Common/...>. The file set's base dir is what
# consumers get on their include path, so only the module root is exposed -
# never Source/Modules/Common itself.
target_sources(XenCommon PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${MODULES_ROOT}
        FILES
        DateTime.hpp
        Exception.hpp
        Hash.hpp
        Ini.hpp
        Io.hpp
        Log.hpp
        Math.hpp
        Platform.hpp
        PrettySignature.hpp
        XenCommon.hpp
)

# Vendored headers that Common's own public headers include (Ini.hpp -> <ini.h>).
target_sources(XenCommon PUBLIC
        FILE_SET vendor
        TYPE HEADERS
        BASE_DIRS ${VENDOR_ROOT}
        FILES
        ${VENDOR_ROOT}/ini.h
)

target_compile_features(XenCommon PUBLIC cxx_std_20)
target_compile_definitions(XenCommon PUBLIC
        XEN_ENGINE_VERSION="dev"
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)
