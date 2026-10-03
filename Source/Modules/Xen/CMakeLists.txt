add_library(Xen STATIC
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
        LightCulling.cpp
        LoadingScreen.cpp
        MeshCache.cpp
        MeshRenderer.cpp
        MipGenerator.cpp
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
        Components/DirectionalLightComponent.cpp
        Components/EnvironmentComponent.cpp
        Components/FPPlayerController.cpp
        Components/MeshComponent.cpp
        Components/PBRMaterialComponent.cpp
        Components/PointLightComponent.cpp
        Components/PostProcessComponent.cpp
        Components/SpotLightComponent.cpp
        Components/SpriteComponent.cpp
        # Engine/Backends - Windows + D3D12 only; the engine has no other target platform.
        Backends/D3D12RenderDevice.cpp
)
add_library(Xen::Xen ALIAS Xen)

# Public headers, included as <Xen/...>. Headers in subfolders must include
# their parent folder's headers as <Xen/...> too - Source/Modules/Xen itself
# is deliberately not on any consumer's include path.
target_sources(Xen PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${MODULES_ROOT}
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
        Input.hpp
        LightCulling.hpp
        LightData.hpp
        LoadingScreen.hpp
        MaterialBindings.hpp
        MeshCache.hpp
        MeshRenderer.hpp
        MipGenerator.hpp
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
        Components/DirectionalLightComponent.hpp
        Components/EnvironmentComponent.hpp
        Components/FPPlayerController.hpp
        Components/MeshComponent.hpp
        Components/PBRMaterialComponent.hpp
        Components/PointLightComponent.hpp
        Components/PostProcessComponent.hpp
        Components/SpotLightComponent.hpp
        Components/SpriteComponent.hpp
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

target_link_libraries(Xen PUBLIC
        Xen::Common
        Xen::PAK
        Microsoft::DirectX-Headers
        Microsoft::DirectX-Guids
        D3D12MemoryAllocator
        d3d12.lib
        dxgi.lib
        dxguid.lib
        d3dcompiler.lib
)
