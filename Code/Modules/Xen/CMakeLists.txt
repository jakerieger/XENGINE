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
        AmbientOcclusionComponent.cpp
        AmbientOcclusionComponent.hpp
        AntiAliasingComponent.cpp
        AntiAliasingComponent.hpp
        AssetLoader.cpp
        AssetLoader.hpp
        AssetPreloader.cpp
        AssetPreloader.hpp
        AssetSettings.hpp
        AudioSourceComponent.cpp
        AudioSourceComponent.hpp
        CameraComponent.cpp
        CameraComponent.hpp
        CommandBuffer.hpp
        Component.hpp
        ComponentRegistry.cpp
        ComponentRegistry.hpp
        DebugUI.cpp
        DebugUI.hpp
        DirectionalLightComponent.cpp
        DirectionalLightComponent.hpp
        EngineConfig.hpp
        EngineContext.hpp
        EnvironmentBaker.cpp
        EnvironmentBaker.hpp
        EnvironmentComponent.cpp
        EnvironmentComponent.hpp
        FPPlayerController.cpp
        FPPlayerController.hpp
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
        MeshComponent.cpp
        MeshComponent.hpp
        MeshRenderer.cpp
        MeshRenderer.hpp
        MipGenerator.cpp
        MipGenerator.hpp
        PBRMaterialComponent.cpp
        PBRMaterialComponent.hpp
        PostProcess.cpp
        PostProcess.hpp
        PostProcessComponent.cpp
        PostProcessComponent.hpp
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
        SpriteComponent.cpp
        SpriteComponent.hpp
        SpriteRenderer.cpp
        SpriteRenderer.hpp
        SpriteBatcher.cpp
        SpriteBatcher.hpp
        SSAO.cpp
        SSAO.hpp
        TextureCache.cpp
        TextureCache.hpp
        Transform.hpp
        Viewport.cpp
        Viewport.hpp
        Window.cpp
        Window.hpp
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