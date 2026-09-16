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

FetchContent_MakeAvailable(
        DirectX-Headers
        D3D12MemoryAllocator
        lz4
        CLI11
)

# D3D12MemoryAllocator's own CMakeLists doesn't know about DirectX-Headers - it only
# reads D3D12MA_USING_DIRECTX_HEADERS as a preprocessor macro inside D3D12MemAlloc.h,
# switching its #include between <directx/d3d12.h> (open-source headers) and the
# Windows SDK's <d3d12.h>. The macro must stay PUBLIC so it reaches Xen too (Xen
# includes D3D12MemAlloc.h and needs the same branch selected).
#
# For the include path, deliberately NOT target_link_libraries(... Microsoft::DirectX-Headers):
# D3D12MemoryAllocator's own CMakeLists (unconditionally, not something we control) does
# install(TARGETS D3D12MemoryAllocator EXPORT ...), and for a static library, any target
# named in target_link_libraries - PUBLIC or PRIVATE alike - becomes part of what that
# export requires (static libs carry their link dependencies forward to whatever finally
# links them, so CMake tracks that regardless of visibility). That breaks the moment we
# name a target (DirectX-Headers) that isn't itself exported. Pulling just the include
# directory via a generator expression gives D3D12MemAlloc.cpp what it needs to compile
# without creating that link-graph edge at all. D3D12MemoryAllocator already links the
# classic dxguid.lib itself (unconditionally, in its own CMakeLists), which provides the
# same GUID symbols DirectX-Guids would - no need to add that one either.
target_compile_definitions(D3D12MemoryAllocator PUBLIC D3D12MA_USING_DIRECTX_HEADERS)
target_include_directories(D3D12MemoryAllocator PRIVATE
        $<TARGET_PROPERTY:Microsoft::DirectX-Headers,INTERFACE_INCLUDE_DIRECTORIES>
)
