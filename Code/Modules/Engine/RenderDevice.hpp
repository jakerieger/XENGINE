//
// Created by Jake Rieger on 9/11/2026.
//

#pragma once

#include "CommandBuffer.hpp"
#include "RHI.hpp"

#include <memory>

namespace Xen::RHI {
    enum class Backend : u8 {
        OpenGL,
        Vulkan,
        D3D12,
        D3D11,
        Null,
    };

    struct DeviceDescriptor {
        Backend API {Backend::OpenGL};

        /// GL: installs the KHR_debug callback and validates handles.
        bool EnableValidation {false};
        /// GL: enables glObjectLabel and glPushDebugGroup, so RenderDoc
        /// captures come out labelled. Cheap; leave on outside of shipping.
        bool EnableDebugMarkers {false};

        /// Ring depth for transient memory. 3 means a BeginFrame almost never
        /// blocks waiting on a fence.
        u32 FramesInFlight {3};
        u32 TransientBufferSize {8 * 1024 * 1024};  // per frame in flight

        /// Off by default, because it only works when the window was created
        /// with GLFW_SRGB_CAPABLE and the textures were uploaded as sRGB.
        /// Turning it on without both produces washed-out colour.
        bool EnableSrgbFramebuffer {false};

        void (*ErrorCallback)(const char* Message, void* UserData) {nullptr};
        void* ErrorUserData {nullptr};
    };

    struct FrameStats {
        u32 DrawCalls {0};
        u32 CommandsExecuted {0};
        u32 PipelineBinds {0};
        u32 RedundantBindsSkipped {0};
        u32 RenderPasses {0};
        u32 TransientBytesUsed {0};
    };

    class IRenderDevice {
    public:
        virtual ~IRenderDevice() = default;

        // --- Lifetime -----------------------------------------------------

        /// @brief Call after the window's context is current and backend is
        /// loaded. Window must therefore be constructed first.
        virtual bool Initialize(const DeviceDescriptor& Desc) = 0;
        virtual void Shutdown()                               = 0;

        _NoDiscard virtual Backend GetBackend() const        = 0;
        _NoDiscard virtual const DeviceCaps& GetCaps() const = 0;

        // --- Resources ----------------------------------------------------
        virtual BufferHandle CreateBuffer(const BufferDesc& Desc)                       = 0;
        virtual TextureHandle CreateTexture(const TextureDesc& Desc)                    = 0;
        virtual SamplerHandle CreateSampler(const SamplerDesc& Desc)                    = 0;
        virtual ShaderHandle CreateShader(const ShaderDesc& Desc)                       = 0;
        virtual PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& Desc) = 0;
        virtual PipelineHandle CreateComputePipeline(const ComputePipelineDesc& Desc)   = 0;

        /// @brief Destruction frees the handle immediately but defers the real
        /// GPU delete until no in-flight frame can still reference it, so it is
        /// safe to destroy a resource the same frame it was drawn with.
        virtual void DestroyBuffer(BufferHandle Handle)     = 0;
        virtual void DestroyTexture(TextureHandle Handle)   = 0;
        virtual void DestroySampler(SamplerHandle Handle)   = 0;
        virtual void DestroyShader(ShaderHandle Handle)     = 0;
        virtual void DestroyPipeline(PipelineHandle Handle) = 0;

        // --- Immediate operations (outside the command stream) ------------
        virtual void UploadTexture(TextureHandle Handle, const TextureUploadDesc& Upload)      = 0;
        virtual void UpdateBuffer(BufferHandle Handle, u64 Offset, const void* Data, u64 Size) = 0;
        virtual void* MapBuffer(BufferHandle Handle, u64 Offset, u64 Size)                     = 0;
        virtual void UnmapBuffer(BufferHandle Handle)                                          = 0;

        // --- Frame --------------------------------------------------------

        /// @brief Rotates the transient ring and waits on the fence for the
        /// frame slot about to be reused.
        virtual void BeginFrame()                          = 0;
        virtual void Submit(const CommandBuffer& Commands) = 0;
        virtual void EndFrame()                            = 0;

        /// @brief Tells the device how large the default framebuffer is. Drive
        /// this from Window::ConsumeResized, and call it once at startup.
        virtual void SetSwapChainSize(u32 Width, u32 Height) = 0;

        _NoDiscard virtual u32 GetSwapChainWidth() const  = 0;
        _NoDiscard virtual u32 GetSwapChainHeight() const = 0;

        /// @brief Sub-allocates from this frame's persistently mapped ring.
        /// The returned pointer is write-only and valid for this frame only.
        virtual TransientAllocation AllocateTransient(u32 Size, BufferUsage Usage) = 0;

        template<typename T>
        TransientAllocation AllocateUniform(const T& Value) {
            const TransientAllocation Alloc = AllocateTransient(sizeof(T), BufferUsage::Uniform);
            if (Alloc.IsValid()) std::memcpy(Alloc.Data, &Value, sizeof(T));
            return Alloc;
        }

        _NoDiscard virtual const FrameStats& GetLastFrameStats() const = 0;
    };

    std::unique_ptr<IRenderDevice> CreateRenderDevice(Backend API);
}  // namespace Xen::RHI