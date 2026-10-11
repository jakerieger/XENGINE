# The engine DLL (Xen.dll / Xend.dll). One shared copy of the
# engine per process - games, XED and the game modules XED loads all use the
# same component registry, logger and ImGui context, and objects can cross
# module boundaries freely.
add_library(Xen SHARED
        # Vendor - compiled into Xen once; games and tools get these symbols by linking Xen::Xen
        ${VENDOR_ROOT}/stb_image.cpp
        ${VENDOR_ROOT}/cgltf.cpp
        ${IMGUI_SOURCES}
        # Engine
        Actor.cpp
        AssetLoader.cpp
        AssetPreloader.cpp
        Color.cpp
        ComponentRegistry.cpp
        DebugUI.cpp
        EnvironmentBaker.cpp
        FXAA.cpp
        Game.cpp
        GameModule.cpp
        LightCulling.cpp
        LoadingScreen.cpp
        MeshCache.cpp
        MeshRenderer.cpp
        MipGenerator.cpp
        Physics/PhysicsWorld.cpp
        PostProcess.cpp
        RadianceHdr.cpp
        RHI.cpp
        Scene.cpp
        SceneSerializer.cpp
        ShaderHotReload.cpp
        SpriteRenderer.cpp
        SpriteBatcher.cpp
        SSAO.cpp
        TAA.cpp
        TextureCache.cpp
        Viewport.cpp
        Window.cpp
        # Engine/Components - concrete IComponent implementations (Component.hpp/
        # ComponentRegistry.* are the base interface + registry, not a
        # component themselves, so they stay at the module root).
        Components/AmbientOcclusionComponent.cpp
        Components/AntiAliasingComponent.cpp
        Components/AudioSourceComponent.cpp
        Components/CameraComponent.cpp
        Components/CharacterControllerComponent.cpp
        Components/DirectionalLightComponent.cpp
        Components/EnvironmentComponent.cpp
        Components/FPPlayerController.cpp
        Components/MeshComponent.cpp
        Components/PBRMaterialComponent.cpp
        Components/PointLightComponent.cpp
        Components/PostProcessComponent.cpp
        Components/PrefabSpawnerComponent.cpp
        Components/RigidBodyComponent.cpp
        Components/SpotLightComponent.cpp
        Components/SpriteComponent.cpp
        # Engine/Backends - Windows + D3D12 only; the engine has no other target platform.
        Backends/D3D12RenderDevice.cpp
)
add_library(Xen::Xen ALIAS Xen)

# Exports every function from the engine's own objects (Common and PAK
# included) without per-class annotations. Global *data* still needs an
# explicit dllimport on the consumer side - the only such case today is
# Dear ImGui's GImGui, handled via IMGUI_API below.
set_target_properties(Xen PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Public headers, included as <Xen/...>. Headers in subfolders must include
# their parent folder's headers as <Xen/...> too - Source/Engine/Xen itself
# is deliberately not on any consumer's include path.
target_sources(Xen PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${ENGINE_ROOT}
        FILES
        Actor.hpp
        ActorHandle.hpp
        AssetLoader.hpp
        AssetPreloader.hpp
        AssetSettings.hpp
        Color.hpp
        CommandBuffer.hpp
        Component.hpp
        ComponentRegistry.hpp
        DebugUI.hpp
        EngineConfig.hpp
        EngineContext.hpp
        EnvironmentBaker.hpp
        FXAA.hpp
        Game.hpp
        GameModule.hpp
        Input.hpp
        LightCulling.hpp
        LightData.hpp
        LoadingScreen.hpp
        MaterialBindings.hpp
        MeshCache.hpp
        MeshRenderer.hpp
        MipGenerator.hpp
        Module.hpp
        PlaceholderComponent.hpp
        PostProcess.hpp
        Reflection.hpp
        RenderDevice.hpp
        RadianceHdr.hpp
        RHI.hpp
        Scene.hpp
        UIOverlay.hpp
        SceneSerializer.hpp
        ShaderHotReload.hpp
        SpriteRenderer.hpp
        SpriteBatcher.hpp
        SSAO.hpp
        TAA.hpp
        TextureCache.hpp
        Transform.hpp
        Viewport.hpp
        Window.hpp
        Components/AmbientOcclusionComponent.hpp
        Components/AntiAliasingComponent.hpp
        Components/AudioSourceComponent.hpp
        Components/CameraComponent.hpp
        Components/CharacterControllerComponent.hpp
        Components/DirectionalLightComponent.hpp
        Components/EnvironmentComponent.hpp
        Components/FPPlayerController.hpp
        Components/MeshComponent.hpp
        Components/PBRMaterialComponent.hpp
        Components/PointLightComponent.hpp
        Components/PostProcessComponent.hpp
        Components/PrefabSpawnerComponent.hpp
        Components/RigidBodyComponent.hpp
        Components/SpotLightComponent.hpp
        Components/SpriteComponent.hpp
        Physics/PhysicsWorld.hpp
        Backends/D3D12Translate.hpp
        Backends/D3D12RenderDevice.hpp
)

# Vendored headers games use directly: nlohmann/json via SceneSerializer.hpp,
# and Dear ImGui (included flat - see Vendor.cmake).
target_sources(Xen PUBLIC
        FILE_SET vendor
        TYPE HEADERS
        BASE_DIRS ${VENDOR_ROOT}
        FILES
        ${VENDOR_ROOT}/nlohmann/json.hpp
)
target_sources(Xen PUBLIC
        FILE_SET imgui
        TYPE HEADERS
        BASE_DIRS ${VENDOR_ROOT}/imgui
        FILES
        ${IMGUI_HEADERS}
        ${IMGUI_BACKEND_HEADERS}
)
target_include_directories(Xen PUBLIC
        $<BUILD_INTERFACE:${VENDOR_ROOT}/imgui/backends>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/XenVendor/backends>
)

# stb_image/cgltf are implementation details - no public header includes them.
target_include_directories(Xen PRIVATE ${VENDOR_ROOT})

# Dear ImGui is compiled into this DLL, and imgui_internal.h's inline code
# reads the GImGui global directly - so consumers must import it.
target_compile_definitions(Xen
        PRIVATE "IMGUI_API=__declspec(dllexport)"
        INTERFACE "IMGUI_API=__declspec(dllimport)"
)

# Common and PAK are object libraries compiled into this DLL (PRIVATE: their
# objects land here, not in consumers). Their public interface - headers and
# compile definitions - is re-exported from Xen below, since consumers only
# ever link Xen::Xen. lz4 rides along privately with XenPAK.
target_link_libraries(Xen PRIVATE XenCommon XenPAK)

# Jolt Physics is an implementation detail of Physics/PhysicsWorld.cpp: linked
# into the DLL, with no Jolt type in any public header. BUILD_INTERFACE keeps
# the (static) Jolt target out of XenTargets' link interface - otherwise the
# export would require Jolt to be exported/installed too (see FetchDeps.cmake's
# D3D12MemoryAllocator comment for the same trap).
target_link_libraries(Xen PRIVATE $<BUILD_INTERFACE:Jolt>)
foreach (_lib XenCommon XenPAK)
    get_target_property(_headers ${_lib} HEADER_SET)
    target_sources(Xen PUBLIC FILE_SET HEADERS BASE_DIRS ${ENGINE_ROOT} FILES ${_headers})
endforeach ()
get_target_property(_common_vendor_headers XenCommon HEADER_SET_vendor)
target_sources(Xen PUBLIC FILE_SET vendor FILES ${_common_vendor_headers})
get_target_property(_common_definitions XenCommon INTERFACE_COMPILE_DEFINITIONS)
target_compile_definitions(Xen PUBLIC ${_common_definitions})
target_compile_features(Xen PUBLIC cxx_std_20)
unset(_lib)
unset(_headers)
unset(_common_vendor_headers)
unset(_common_definitions)

target_link_libraries(Xen PUBLIC
        Microsoft::DirectX-Headers
        Microsoft::DirectX-Guids
        D3D12MemoryAllocator
        d3d12.lib
        dxgi.lib
        dxguid.lib
        d3dcompiler.lib
)
