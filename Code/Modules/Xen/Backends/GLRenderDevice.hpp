//
// Created by Jake Rieger on 9/11/2026.
//

#pragma once

#define GLFW_INCLUDE_NONE
#include <glad.h>

#include "../RenderDevice.hpp"

#include <array>
#include <unordered_map>
#include <vector>

namespace Xen::RHI::GL {
    struct GLBuffer {
        GLuint ID {0};
        u64 Size {0};
        BufferUsage Usage {BufferUsage::None};
        MemoryUsage Memory {MemoryUsage::GpuOnly};
        void* Mapped {nullptr};  // non-null when persistently mapped
        bool Transient {false};  // owned by the ring, not by a Destroy call
    };

    struct GLTexture {
        GLuint ID {0};
        GLenum Target {GL_TEXTURE_2D};
        Format Fmt {Format::Unknown};
        u32 Width {0};
        u32 Height {0};
        u32 ArrayLayers {1};
        u32 MipLevels {1};
        u32 SampleCount {1};
        TextureUsage Usage {TextureUsage::None};
    };

    struct GLSampler {
        GLuint ID {0};
    };

    struct GLShader {
        GLuint ID {0};
        ShaderStage Stage {ShaderStage::Vertex};
    };

    struct GLPipeline {
        GLuint Program {0};
        GLuint VAO {0};  // shared, cached per vertex layout
        VertexLayout Layout {};
        GLenum Topology {GL_TRIANGLES};
        RasterizerState Rasterizer {};
        DepthStencilState DepthStencil {};
        BlendState Blend {};
        bool IsCompute {false};
        /// Cached so BindVertexBuffer does not have to walk the layout.
        std::array<u16, MAX_VERTEX_BUFFERS> Strides {};
    };

    /// @brief Redundant-state filter.
    ///
    /// The driver does some of this, but not reliably and not for free: every
    /// glEnable is a validated call across the driver boundary.
    struct GLStateCache {
        GLuint Program {0};
        GLuint VAO {0};
        GLuint Framebuffer {0};

        bool DepthTest {false};
        bool DepthWrite {false};
        GLenum DepthFunc {GL_LESS};
        bool StencilTest {false};

        bool CullEnabled {false};
        GLenum CullFace {GL_BACK};
        GLenum FrontFace {GL_CCW};
        GLenum PolygonMode {GL_FILL};
        bool ScissorTest {false};
        f32 LineWidth {1.0f};

        bool BlendEnabled[MAX_COLOR_ATTACHMENTS] {};
        BlendAttachmentState BlendState_[MAX_COLOR_ATTACHMENTS] {};
        bool IndependentBlend {false};

        Viewport View {};
        ScissorRect Scissor {};

        std::array<GLuint, 32> TextureUnits {};
        std::array<GLuint, 32> SamplerUnits {};

        /// @brief Resets to defaults and pokes the object bindings to
        /// impossible values so the next bind always goes through. Scalar
        /// state is handled by GLRenderDevice::_ForceStateApply, because a
        /// default-valued cache entry can otherwise match a request while the
        /// real GL state differs.
        void Invalidate() {
            *this       = GLStateCache {};
            Program     = 0xFFFFFFFF;
            VAO         = 0xFFFFFFFF;
            Framebuffer = 0xFFFFFFFF;
        }
    };

    /// @brief Per-frame linear allocator over a persistently mapped, coherent
    /// ring buffer.
    ///
    /// One arena per frame in flight. BeginFrame waits on the fence for the
    /// arena about to be reused, which with three frames almost never blocks.
    /// This is what makes per-frame uniform and instance data free: no buffer
    /// orphaning, no glBufferSubData stall.
    class TransientRing {
    public:
        bool Initialize(u32 BytesPerFrame, u32 FramesInFlight, u32 UboAlign, u32 SsboAlign);
        void Shutdown();

        void BeginFrame(u32 FrameIndex);
        void EndFrame(u32 FrameIndex);

        TransientAllocation Allocate(u32 Size, BufferUsage Usage, BufferHandle ArenaHandle);

        NODISCARD BufferHandle GetHandle(const u32 FrameIndex) const { return _Handles[FrameIndex]; }
        NODISCARD GLuint GetBufferID(const u32 FrameIndex) const { return _Arenas[FrameIndex].ID; }
        void SetHandle(const u32 FrameIndex, const BufferHandle H) { _Handles[FrameIndex] = H; }
        NODISCARD u32 GetBytesUsed() const { return _Head; }

    private:
        struct Arena {
            GLuint ID {0};
            u8* Mapped {nullptr};
            GLsync Fence {nullptr};
        };

        std::vector<Arena> _Arenas;
        std::vector<BufferHandle> _Handles;
        u32 _Capacity {0};
        u32 _Head {0};
        u32 _FrameIndex {0};
        u32 _UboAlign {256};
        u32 _SsboAlign {256};
    };

    class GLRenderDevice final : public IRenderDevice {
    public:
        GLRenderDevice();
        ~GLRenderDevice() override;

        bool Initialize(const DeviceDescriptor& Desc) override;
        void Shutdown() override;

        NODISCARD Backend GetBackend() const override { return Backend::OpenGL; }
        NODISCARD const DeviceCaps& GetCaps() const override { return _Caps; }

        BufferHandle CreateBuffer(const BufferDesc& Desc) override;
        TextureHandle CreateTexture(const TextureDesc& Desc) override;
        SamplerHandle CreateSampler(const SamplerDesc& Desc) override;
        ShaderHandle CreateShader(const ShaderDesc& Desc) override;
        PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& Desc) override;
        PipelineHandle CreateComputePipeline(const ComputePipelineDesc& Desc) override;

        void DestroyBuffer(BufferHandle Handle) override;
        void DestroyTexture(TextureHandle Handle) override;
        void DestroySampler(SamplerHandle Handle) override;
        void DestroyShader(ShaderHandle Handle) override;
        void DestroyPipeline(PipelineHandle Handle) override;

        void UploadTexture(TextureHandle Handle, const TextureUploadDesc& Upload) override;
        void UpdateBuffer(BufferHandle Handle, u64 Offset, const void* Data, u64 Size) override;
        void* MapBuffer(BufferHandle Handle, u64 Offset, u64 Size) override;
        void UnmapBuffer(BufferHandle Handle) override;

        void BeginFrame() override;
        void Submit(const CommandBuffer& Commands) override;
        void EndFrame() override;

        void SetSwapChainSize(u32 Width, u32 Height) override;
        NODISCARD u32 GetSwapChainWidth() const override { return _SwapWidth; }
        NODISCARD u32 GetSwapChainHeight() const override { return _SwapHeight; }

        TransientAllocation AllocateTransient(u32 Size, BufferUsage Usage) override;

        NODISCARD const FrameStats& GetLastFrameStats() const override { return _LastStats; }

    private:
        void ExecuteBeginRenderPass(const RenderPassDesc& Desc);
        void ExecuteEndRenderPass();
        void ApplyPipeline(const GLPipeline& Pipeline);
        void ApplyRasterizer(const RasterizerState& State);
        void ApplyDepthStencil(const DepthStencilState& State);
        void ApplyBlend(const BlendState& State);
        void FlushVertexState();

        GLuint GetOrCreateVAO(const VertexLayout& Layout);
        GLuint GetOrCreateFBO(const RenderPassDesc& Desc);

        enum class GLObjectType : u8 { Buffer, Texture, Sampler, Shader, Program, VAO, FBO };
        struct PendingDelete {
            GLObjectType Type;
            GLuint ID;
            u32 Frame;
        };
        void EnqueueDelete(GLObjectType Type, GLuint ID);
        void ProcessDeletions();

        void Log(bool Error, const char* Fmt, ...) const;
        static void APIENTRY DebugCallback(GLenum Source,
                                           GLenum Type,
                                           GLuint ID,
                                           GLenum Severity,
                                           GLsizei Length,
                                           const GLchar* Message,
                                           const void* UserParam);

        DeviceDescriptor _Desc {};
        DeviceCaps _Caps {};

        Pool<GLBuffer, BufferHandle> _Buffers;
        Pool<GLTexture, TextureHandle> _Textures;
        Pool<GLSampler, SamplerHandle> _Samplers;
        Pool<GLShader, ShaderHandle> _Shaders;
        Pool<GLPipeline, PipelineHandle> _Pipelines;

        std::unordered_map<u64, GLuint> _VAOCache;  // hash(VertexLayout) -> VAO
        std::unordered_map<u64, GLuint> _FBOCache;  // hash(attachments)  -> FBO

        GLStateCache _State {};
        TransientRing _Transient;

        bool _ForceStateApply {true};

        const GLPipeline* _CurrentPipeline {nullptr};
        GLenum _IndexType {GL_UNSIGNED_INT};
        GLsizei _IndexSize {4};
        u64 _IndexOffset {0};
        u32 _StencilRef {0};

        RenderPassDesc _CurrentPass {};
        GLuint _CurrentPassFBO {0};
        bool _InRenderPass {false};

        // Vertex and index bindings are applied lazily at draw time: the
        // stride comes from the pipeline, and the VAO they attach to can
        // change after the bind command was recorded. Buffering them here
        // makes bind order irrelevant to the caller.
        struct PendingVertexBuffer {
            GLuint Buffer {0};
            GLintptr Offset {0};
        };
        std::array<PendingVertexBuffer, MAX_VERTEX_BUFFERS> _PendingVB {};
        u32 _VertexBufferDirtyMask {0};
        GLuint _PendingIndexBuffer {0};
        bool _IndexBufferDirty {false};

        u32 _FrameIndex {0};
        u32 _FrameCounter {0};
        u32 _SwapWidth {0};
        u32 _SwapHeight {0};
        FrameStats _Stats {};
        FrameStats _LastStats {};

        std::vector<PendingDelete> _PendingDeletes;
        bool _Initialized {false};
    };
}  // namespace Xen::RHI::GL