# OBJECT library, like XenCommon: compiled into the engine DLL and PAKTool.
add_library(XenPAK OBJECT
        Aes.cpp
        AssetMount.cpp
        AssetRegistry.cpp
        BinaryIO.cpp
        Canonicalize.cpp
        Codec.cpp
        ContentScanner.cpp
        Crypto.cpp
        LooseFileSource.cpp
        PakFileSource.cpp
        PakFormat.cpp
        PakManifest.cpp
)
add_library(Xen::PAK ALIAS XenPAK)

# Public headers, included as <XenPAK/...>.
target_sources(XenPAK PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${ENGINE_ROOT}
        FILES
        Aes.hpp
        AssetBuffer.hpp
        AssetID.hpp
        AssetMount.hpp
        AssetRegistry.hpp
        AssetSource.hpp
        BinaryIO.hpp
        Canonicalize.hpp
        Codec.hpp
        ContentScanner.hpp
        Crypto.hpp
        LooseFileSource.hpp
        PakFileSource.hpp
        PakFormat.hpp
        PakManifest.hpp
)

# Common is PUBLIC because XenPAK's headers include <Common/...>; lz4 is an
# implementation detail of Codec.cpp only.
target_link_libraries(XenPAK
        PUBLIC Xen::Common
        PRIVATE lz4_static
)

# nlohmann/json, used privately by PakManifest.cpp.
target_include_directories(XenPAK PRIVATE ${VENDOR_ROOT})
