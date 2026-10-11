# OBJECT library: compiled into the engine DLL (Xen) and into PAKTool, which
# stays independent of the engine DLL. Never installed on its own - Xen
# re-exports its headers and public definitions (see Xen/CMakeLists.txt).
add_library(XenCommon OBJECT
        Io.cpp
        Log.cpp
)
add_library(Xen::Common ALIAS XenCommon)

# Public headers, included as <Common/...>. The file set's base dir is what
# consumers get on their include path, so only the module root is exposed -
# never Source/Engine/Common itself.
target_sources(XenCommon PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${ENGINE_ROOT}
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
# XEN_ENGINE_VERSION comes from the top-level project(XENGINE VERSION ...) -
# the same version find_package(Xen <version>) checks, and the one XED stamps
# into (and requires of) every .prxj project file.
target_compile_definitions(XenCommon PUBLIC
        XEN_ENGINE_VERSION="${XENGINE_VERSION}"
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)
