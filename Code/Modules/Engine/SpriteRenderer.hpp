//
// Created by Jake Rieger on 9/11/2026.
//
// Turns the draw list SpriteBatcher builds into RHI commands.

#pragma once

#include "EngineCommon.hpp"
#include "RenderDevice.hpp"
#include "SpriteBatcher.hpp"
#include "TextureCache.hpp"

#include <glm/glm.hpp>

namespace Xen {
    class SpriteRenderer {
    public:
        struct Config {
            RHI::FilterMode Filter {RHI::FilterMode::Nearest};
            u32 MaxSpritesPerFrame {65536};
            glm::vec4 ClearColor {0.1f, 0.1f, 0.1f, 1.0f};

            Config() {}
        };

        SpriteRenderer() = default;
        ~SpriteRenderer();

        SpriteRenderer(const SpriteRenderer&)            = delete;
        SpriteRenderer& operator=(const SpriteRenderer&) = delete;

        /// @brief Builds the pipeline and sampler. Requires an initialized
        /// device, so call it after IRenderDevice::Initialize.
        bool Initialize(RHI::IRenderDevice& Device, const Config& Cfg = {});
        void Shutdown();

        _NoDiscard bool IsInitialized() const { return _Device != nullptr; }

        /// @brief Records and submits one frame's sprites. Call between
        /// IRenderDevice::BeginFrame and EndFrame.
        ///
        /// Always opens a render pass, even with an empty draw list, so the
        /// back buffer still gets cleared on a frame with nothing visible.
        void Render(const SpriteBatcher& Batcher, const TextureCache& Textures);

        void SetClearColor(const glm::vec4& Color) { _Config.ClearColor = Color; }

        _NoDiscard u32 GetSpritesSubmitted() const { return _SpritesSubmitted; }
        _NoDiscard u32 GetSpritesDropped() const { return _SpritesDropped; }

    private:
        /// @brief One quad's worth of per-instance data.
        ///
        /// The quad's four corners come from gl_VertexID in the vertex shader,
        /// so there is no per-vertex buffer at all: the whole frame is this
        /// array, written straight into mapped memory.
        struct SpriteInstance {
            glm::vec2 Center;  // world units
            glm::vec2 Size;    // world units, signed - negative flips
            f32 Rotation;      // radians
            f32 _Pad;
            glm::vec4 UVRect;  // xy = min uv, zw = uv size
            glm::vec4 Tint;
        };
        static_assert(sizeof(SpriteInstance) == 56, "SpriteInstance layout must match the shader");

        struct FrameUniforms {
            glm::mat4 ViewProjection;
        };

        RHI::IRenderDevice* _Device {nullptr};
        Config _Config {};

        RHI::PipelineHandle _Pipeline {};
        RHI::SamplerHandle _Sampler {};
        RHI::CommandBuffer _Commands;

        u32 _SpritesSubmitted {0};
        u32 _SpritesDropped {0};
    };
}  // namespace Xen
