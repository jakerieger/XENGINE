//
// Created by Jake Rieger on 9/11/2026.
//

#pragma once

#include "CommandBuffer.hpp"
#include "RHI.hpp"

#include <memory>
#include <vector>

namespace Xen::RHI {
    enum class Backend : u8 {
        OpenGL,
        Vulkan,
        D3D12,
        D3D11,
        Null,
    };

    struct DeviceDescriptor {
        Backend API {Backend::D3D12};

        /// @brief The native window handle to create the swap chain against
        /// (an HWND, opaque here so this header stays platform-agnostic).
        /// Required by any backend that owns its own presentation surface
        /// (D3D12); a backend that instead attaches to a context the window
        /// already made current wouldn't need this at all.
        void* NativeWindowHandle {nullptr};

        /// D3D12: enables the debug layer.
        bool EnableValidation {false};
        /// D3D12: enables PIX/RenderDoc-visible object/event labelling. Cheap;
        /// leave on outside of shipping.
        bool EnableDebugMarkers {false};

        /// Ring depth for transient memory. 3 means a BeginFrame almost never
        /// blocks waiting on a fence.
        u32 FramesInFlight {3};
        u32 TransientBufferSize {8 * 1024 * 1024};  // per frame in flight

        /// Off by default, because it only works when the swap chain format is
        /// an sRGB variant and the textures were uploaded as sRGB. Turning it
        /// on without both produces washed-out colour.
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
        // Only tallied for TriangleList/TriangleStrip draws - point/line
        // topologies leave this untouched, since they draw no triangles.
        u32 TriangleCount {0};
    };

    /// @brief One named GPU timing scope - a CommandBuffer::PushDebugGroup/
    /// PopDebugGroup pair, measured with GPU timestamp queries rather than a
    /// CPU clock (the two can diverge a lot: the CPU may have moved on to
    /// recording next frame's commands long before the GPU actually executes
    /// this one). See IRenderDevice::GetLastFrameGpuTimings.
    struct GpuScopeTiming {
        char Name[32] {};
        f32 Milliseconds {0.0f};
        // Nesting depth (0 = a top-level scope) - PushDebugGroup calls can
        // nest (e.g. "SSAO" inside "Meshes"), purely for a debug UI to
        // indent by; this backend doesn't otherwise care about nesting.
        u32 Depth {0};
    };

    /// @brief Snapshot of GPU/RAM memory usage, queried on demand (not
    /// per-frame tracked like FrameStats) - cheap enough to call from a debug
    /// UI every frame, but not free enough to fold into the hot Submit() path.
    struct MemoryStats {
        /// Bytes actually suballocated to live resources - the useful number.
        u64 GpuAllocatedBytes {0};
        /// Bytes reserved in GPU memory blocks, including unused
        /// suballocation slack; always >= GpuAllocatedBytes.
        u64 GpuReservedBytes {0};
        /// OS-reported total video memory usage attributed to this process
        /// (includes resources outside this allocator, e.g. the swap chain).
        u64 GpuUsageBytes {0};
        /// OS-reported video memory budget/ceiling before the driver starts
        /// evicting this process's allocations - not a hard cap.
        u64 GpuBudgetBytes {0};
        /// Current process working set (RAM) - not GPU-related, but relevant
        /// alongside VRAM for spotting a leak.
        u64 ProcessRamBytes {0};
    };

    class IRenderDevice {
    public:
        virtual ~IRenderDevice() = default;

        // --- Lifetime -----------------------------------------------------

        /// @brief Call after the window's context is current and backend is
        /// loaded. Window must therefore be constructed first.
        virtual bool Initialize(const DeviceDescriptor& Desc) = 0;
        virtual void Shutdown()                               = 0;

        /// @brief Blocks until the GPU has finished every previously-submitted
        /// frame. Callers that are about to destroy resources outside the
        /// normal per-frame Destroy*() deferred-delete path (e.g. tearing down
        /// a scene, or the final shutdown teardown before this device itself is
        /// destroyed) must call this first - otherwise a resource can be
        /// released while the GPU is still reading it.
        virtual void WaitIdle() = 0;

        NODISCARD virtual Backend GetBackend() const        = 0;
        NODISCARD virtual const DeviceCaps& GetCaps() const = 0;

        // --- Resources ----------------------------------------------------
        virtual BufferHandle CreateBuffer(const BufferDesc& Desc)                       = 0;
        virtual TextureHandle CreateTexture(const TextureDesc& Desc)                    = 0;
        virtual SamplerHandle CreateSampler(const SamplerDesc& Desc)                    = 0;
        virtual ShaderHandle CreateShader(const ShaderDesc& Desc)                       = 0;
        virtual LayoutHandle CreatePipelineLayout(const PipelineLayoutDesc& Desc)       = 0;
        virtual PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& Desc) = 0;
        virtual PipelineHandle CreateComputePipeline(const ComputePipelineDesc& Desc)   = 0;

        /// @brief Destruction frees the handle immediately but defers the real
        /// GPU delete until no in-flight frame can still reference it, so it is
        /// safe to destroy a resource the same frame it was drawn with.
        virtual void DestroyBuffer(BufferHandle Handle)         = 0;
        virtual void DestroyTexture(TextureHandle Handle)       = 0;
        virtual void DestroySampler(SamplerHandle Handle)       = 0;
        virtual void DestroyShader(ShaderHandle Handle)         = 0;
        virtual void DestroyPipelineLayout(LayoutHandle Handle) = 0;
        virtual void DestroyPipeline(PipelineHandle Handle)     = 0;

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

        /// @brief Executes Commands immediately and blocks until the GPU has
        /// finished them - outside the BeginFrame/Submit/EndFrame cycle, with
        /// no present. For one-shot work that has to be done before the first
        /// frame or between frames (baking a lookup table into a render
        /// target, say), the same way UploadTexture is already synchronous.
        ///
        /// Must NOT be called between BeginFrame and EndFrame: it runs on its
        /// own command list, so resource-state changes it makes would race
        /// with whatever the in-progress frame's list has already recorded
        /// against the same resources. Frame stats (FrameStats) accumulate
        /// its commands like any other Submit.
        virtual void SubmitAndWait(const CommandBuffer& Commands) = 0;

        /// @brief Tells the device how large the default framebuffer is. Drive
        /// this from Window::ConsumeResized, and call it once at startup.
        virtual void SetSwapChainSize(u32 Width, u32 Height) = 0;

        /// @brief Copies Source into this frame's swap chain back buffer.
        ///
        /// This is the "no shader system yet" way to land a Viewport on
        /// screen: a plain GPU copy rather than a shader blit, which is why
        /// it requires Source to already be in the back buffer's own pixel
        /// format (BGRA8_UNORM) and exactly its size - a raw copy can't
        /// convert either. Once a fullscreen-blit/post-process pipeline
        /// exists this becomes one option for presenting a Viewport, not the
        /// only one (an editor would instead sample it via ImGui::Image and
        /// never call this at all).
        virtual void CopyToSwapChain(TextureHandle Source) = 0;

        NODISCARD virtual u32 GetSwapChainWidth() const  = 0;
        NODISCARD virtual u32 GetSwapChainHeight() const = 0;

        /// @brief Sub-allocates from this frame's persistently mapped ring.
        /// The returned pointer is write-only and valid for this frame only.
        virtual TransientAllocation AllocateTransient(u32 Size, BufferUsage Usage) = 0;

        template<typename T>
        TransientAllocation AllocateUniform(const T& Value) {
            const TransientAllocation Alloc = AllocateTransient(sizeof(T), BufferUsage::Uniform);
            if (Alloc.IsValid()) std::memcpy(Alloc.Data, &Value, sizeof(T));
            return Alloc;
        }

        NODISCARD virtual const FrameStats& GetLastFrameStats() const = 0;

        /// @brief Every CommandBuffer::PushDebugGroup/PopDebugGroup scope's
        /// GPU duration, from the most recently *resolved* frame - not
        /// necessarily last frame: a GPU timestamp query can only be read
        /// back once the GPU has actually finished that work, which a
        /// backend may only guarantee a few frames after it was recorded
        /// (the same latency IRenderDevice::BeginFrame's own frame-in-flight
        /// wait already has). Empty before any frame has fully resolved.
        NODISCARD virtual const std::vector<GpuScopeTiming>& GetLastFrameGpuTimings() const = 0;

        /// @brief Queried, not tracked per-frame - see MemoryStats.
        NODISCARD virtual MemoryStats GetMemoryStats() const = 0;
    };

    std::unique_ptr<IRenderDevice> CreateRenderDevice(Backend API);
}  // namespace Xen::RHI