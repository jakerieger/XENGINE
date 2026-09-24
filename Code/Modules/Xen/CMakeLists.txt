project(XENGINE)

file(GLOB_RECURSE IMGUI_SOURCES
        ${VENDOR_ROOT}/imgui/*.h
        ${VENDOR_ROOT}/imgui/*.cpp
)

add_library(Xen STATIC
        # Vendor
        ${VENDOR_ROOT}/nlohmann/json.hpp
        ${VENDOR_ROOT}/stb_image.cpp
        ${VENDOR_ROOT}/stb_image.h
        ${VENDOR_ROOT}/ini.h
        ${VENDOR_ROOT}/cgltf.cpp
        ${VENDOR_ROOT}/cgltf.h
        ${IMGUI_SOURCES}
        # Engine
        Actor.cpp
        Actor.hpp
        ActorHandle.hpp
        AssetLoader.cpp
        AssetLoader.hpp
        AssetPreloader.cpp
        AssetPreloader.hpp
        AssetSettings.hpp
        CommandBuffer.hpp
        Component.hpp
        ComponentRegistry.cpp
        ComponentRegistry.hpp
        DebugUI.cpp
        DebugUI.hpp
        EngineConfig.hpp
        EngineContext.hpp
        EnvironmentBaker.cpp
        EnvironmentBaker.hpp
        FXAA.cpp
        FXAA.hpp
        Game.cpp
        Game.hpp
        Input.hpp
        LoadingScreen.cpp
        LoadingScreen.hpp
        MaterialBindings.hpp
        MeshCache.cpp
        MeshCache.hpp
        MeshRenderer.cpp
        MeshRenderer.hpp
        MipGenerator.cpp
        MipGenerator.hpp
        PostProcess.cpp
        PostProcess.hpp
        Reflection.hpp
        RenderDevice.hpp
        RadianceHdr.cpp
        RadianceHdr.hpp
        RHI.cpp
        RHI.hpp
        Scene.cpp
        Scene.hpp
        SceneSerializer.cpp
        SceneSerializer.hpp
        ShaderHotReload.cpp
        ShaderHotReload.hpp
        SpriteRenderer.cpp
        SpriteRenderer.hpp
        SpriteBatcher.cpp
        SpriteBatcher.hpp
        SSAO.cpp
        SSAO.hpp
        TAA.cpp
        TAA.hpp
        TextureCache.cpp
        TextureCache.hpp
        Transform.hpp
        Viewport.cpp
        Viewport.hpp
        Window.cpp
        Window.hpp
        # Engine/Components - concrete IComponent implementations (Component.hpp/
        # ComponentRegistry.* above are the base interface + registry, not a
        # component themselves, so they stay at the module root).
        Components/AmbientOcclusionComponent.cpp
        Components/AmbientOcclusionComponent.hpp
        Components/AntiAliasingComponent.cpp
        Components/AntiAliasingComponent.hpp
        Components/AudioSourceComponent.cpp
        Components/AudioSourceComponent.hpp
        Components/CameraComponent.cpp
        Components/CameraComponent.hpp
        Components/DirectionalLightComponent.cpp
        Components/DirectionalLightComponent.hpp
        Components/EnvironmentComponent.cpp
        Components/EnvironmentComponent.hpp
        Components/FPPlayerController.cpp
        Components/FPPlayerController.hpp
        Components/MeshComponent.cpp
        Components/MeshComponent.hpp
        Components/PBRMaterialComponent.cpp
        Components/PBRMaterialComponent.hpp
        Components/PointLightComponent.cpp
        Components/PointLightComponent.hpp
        Components/PostProcessComponent.cpp
        Components/PostProcessComponent.hpp
        Components/SpotLightComponent.cpp
        Components/SpotLightComponent.hpp
        Components/SpriteComponent.cpp
        Components/SpriteComponent.hpp
        # Engine/Backends - Windows + D3D12 only; the engine has no other target platform.
        Backends/D3D12Translate.hpp
        Backends/D3D12RenderDevice.cpp
        Backends/D3D12RenderDevice.hpp
)
add_library(Xen::Xen ALIAS Xen)

target_include_directories(Xen PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${VENDOR_ROOT}>/imgui
        $<BUILD_INTERFACE:${VENDOR_ROOT}>/imgui/backends
)

target_compile_definitions(Xen PUBLIC
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)

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
target_include_directories(Xen PRIVATE ${MODULES_ROOT} ${VENDOR_ROOT})