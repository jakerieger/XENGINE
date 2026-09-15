project(Xen2D)

file(GLOB SHADER_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Include/*.h)

add_library(Xen STATIC
        # Vendor
        ${VENDOR_ROOT}/nlohmann/json.hpp
        ${VENDOR_ROOT}/stb_image.cpp
        ${VENDOR_ROOT}/stb_image.h
        ${VENDOR_ROOT}/glad.c
        ${VENDOR_ROOT}/glad.h
        ${VENDOR_ROOT}/khrplatform.h
        ${VENDOR_ROOT}/INIReader.cpp
        ${VENDOR_ROOT}/INIReader.h
        ${VENDOR_ROOT}/ini.c
        ${VENDOR_ROOT}/ini.h
        # Engine
        ${SHADER_INCLUDES}
        Actor.cpp
        Actor.hpp
        ActorHandle.hpp
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
        EngineConfig.hpp
        EngineContext.hpp
        Game.cpp
        Game.hpp
        Reflection.hpp
        RenderDevice.hpp
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
        TextureCache.cpp
        TextureCache.hpp
        Transform.hpp
        Window.cpp
        Window.hpp
        # Engine/Backends (TODO: dynamically select this based on platform)
        Backends/GLTranslate.hpp
        Backends/GLRenderDevice.cpp
        Backends/GLRenderDevice.hpp
)
add_library(Xen::Xen ALIAS Xen)

target_include_directories(Xen PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(Xen PUBLIC
        Xen::Common
        Xen::PAK
        glm::glm-header-only
        glfw
)
target_include_directories(Xen PRIVATE ${MODULES_ROOT} ${VENDOR_ROOT})