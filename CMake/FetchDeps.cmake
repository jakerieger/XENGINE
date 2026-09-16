include(FetchContent)

FetchContent_Declare(
        DirectX-Headers
        GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
        GIT_TAG v1.615.0
)

FetchContent_Declare(
        D3D12MemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator.git
        GIT_TAG v3.1.0
)

set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        lz4
        URL https://github.com/lz4/lz4/archive/refs/tags/v1.10.0.tar.gz
        URL_HASH SHA256=537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b
        SOURCE_SUBDIR build/cmake
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
        CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.7.2
)

# D3D12MemoryAllocator's CMakeLists installs an export set for itself that
# (once we link DirectX-Headers into it below) would also require
# DirectX-Headers/DirectX-Guids to be part of an export set - which they
# aren't, since they're fetched directly rather than found via
# find_package(). We never install any of this project's dependencies, so
# skipping install() processing entirely for everything FetchContent adds
# here sidesteps that validation rather than fighting it.
set(CMAKE_SKIP_INSTALL_RULES ON)

FetchContent_MakeAvailable(
        DirectX-Headers
        D3D12MemoryAllocator
        lz4
        CLI11
)

# D3D12MemoryAllocator's own CMakeLists doesn't know about DirectX-Headers - it only
# reads D3D12MA_USING_DIRECTX_HEADERS as a preprocessor macro inside D3D12MemAlloc.h,
# switching its #include between <directx/d3d12.h> (open-source headers) and the
# Windows SDK's <d3d12.h>. Wire both the macro and the include path in ourselves.
target_compile_definitions(D3D12MemoryAllocator PUBLIC D3D12MA_USING_DIRECTX_HEADERS)
target_link_libraries(D3D12MemoryAllocator PUBLIC Microsoft::DirectX-Headers Microsoft::DirectX-Guids)