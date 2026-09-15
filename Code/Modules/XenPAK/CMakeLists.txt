project(Xen2D)

add_library(XenPAK STATIC
        Aes.cpp
        Aes.hpp
        AssetBuffer.hpp
        AssetID.hpp
        AssetMount.cpp
        AssetMount.hpp
        AssetRegistry.cpp
        AssetRegistry.hpp
        AssetSource.hpp
        BinaryIO.cpp
        BinaryIO.hpp
        Canonicalize.cpp
        Canonicalize.hpp
        Codec.cpp
        Codec.hpp
        ContentScanner.cpp
        ContentScanner.hpp
        Crypto.cpp
        Crypto.hpp
        LooseFileSource.cpp
        LooseFileSource.hpp
        PakFileSource.cpp
        PakFileSource.hpp
        PakFormat.cpp
        PakFormat.hpp
        PakManifest.cpp
        PakManifest.hpp
)
add_library(Xen::PAK ALIAS XenPAK)

target_link_libraries(XenPAK PRIVATE lz4_static)
target_include_directories(XenPAK PRIVATE ${MODULES_ROOT} ${VENDOR_ROOT})