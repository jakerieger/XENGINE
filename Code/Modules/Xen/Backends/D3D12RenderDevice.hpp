//
// Created by Jake Rieger on 9/15/2026.
//

#pragma once

#include "../RenderDevice.hpp"
#include "D3D12Translate.hpp"

#include <D3D12MemAlloc.h>
#include <wrl/client.h>

#include <array>
#include <deque>
#include <functional>
#include <unordered_map>

namespace Xen::RHI::D3D12Backend {
    using Microsoft::WRL::ComPtr;

    struct D3DBuffer {
        ComPtr<ID3D12Resource> Resource;
        ComPtr<D3D12MA::Allocation> Allocation;
        u64 Size {0};
        BufferUsage Usage {BufferUsage::None};
        MemoryUsage Memory {MemoryUsage::GpuOnly};
        void* Mapped {nullptr};  // non-null for CpuToGpu buffers - persistently mapped
        bool Transient {false};

        // A raw-buffer UAV descriptor, valid for the buffer's whole lifetime
        // once created (BufferUsage::Storage only) - unlike D3DTexture, this
        // engine gives a Storage buffer no other state to be in: it's
        // created directly into D3D12_RESOURCE_STATE_UNORDERED_ACCESS and
        // never transitions away from it (see BindStorageBuffer's executor
        // and CmdType::PipelineBarrier's UAV-hazard barrier), so there's no
        // CurrentState field to track here the way D3DTexture has.
        u32 UavHeapIndex {UINT32_MAX};
    };

    struct D3DTexture {
        ComPtr<ID3D12Resource> Resource;
        ComPtr<D3D12MA::Allocation> Allocation;
        DXGI_FORMAT Format {DXGI_FORMAT_UNKNOWN};
        u32 Width {0};
        u32 Height {0};
        u32 MipLevels {1};
        TextureUsage Usage {TextureUsage::None};
        u32 SrvHeapIndex {UINT32_MAX};
        u32 RtvHeapIndex {UINT32_MAX};  // the first render-target view (mip 0, layer 0) - RtvSlots[0]
        u32 DsvHeapIndex {UINT32_MAX};

        TextureType Type {TextureType::Texture2D};
        u32 Mips {1};    // the resource's actual mip count (Desc.MipLevels == 0 already resolved)
        u32 Layers {1};  // array slices; 6 for a cube

        // One render-target view per (layer, mip), indexed layer * Mips + mip
        // - a render pass targets a single mip of a single slice at a time
        // (ColorAttachment::MipLevel/ArrayLayer). Empty for a texture that
        // isn't a color target.
        std::vector<u32> RtvSlots;

        // Tracked so a texture reused across passes/frames (a render target
        // that's later sampled, then rendered into again next frame) gets the
        // right transition instead of assuming a fixed resting state the way
        // the swap chain's PRESENT<->RENDER_TARGET back buffers can. Sprite
        // textures - created once, uploaded once, sampled forever - never
        // revisit this after their initial transition in UploadTexture.
        D3D12_RESOURCE_STATES CurrentState {D3D12_RESOURCE_STATE_COMMON};
    };

    struct D3DSampler {
        u32 HeapIndex {UINT32_MAX};
    };

    struct D3DShader {
        ComPtr<ID3DBlob> Bytecode;
        ShaderStage Stage {ShaderStage::Vertex};
    };

    /// @brief A root signature built from a PipelineLayoutDesc, plus enough
    /// bookkeeping to resolve Submit()'s BindUniformBuffer(Slot=N)/
    /// BindTexture(Slot=N)/etc. commands back to the right root parameter -
    /// those commands only carry the slot and (by which Cmd:: type they are)
    /// the binding type, not which root parameter index that landed at,
    /// since that's a per-layout backend decision the RHI layer never sees.
    struct D3DPipelineLayout {
        ComPtr<ID3D12RootSignature> RootSignature;

        struct ResolvedBinding {
            u32 Slot {0};
            BindingType Type {BindingType::UniformBuffer};
            u32 RootParameterIndex {0};
            bool IsTable {false};  // false = root descriptor (CBV only), true = descriptor table
        };
        std::array<ResolvedBinding, PipelineLayoutDesc::MAX_BINDINGS> Bindings {};
        u8 BindingCount {0};

        NODISCARD const ResolvedBinding* Find(const u32 Slot, const BindingType Type) const {
            for (u8 i = 0; i < BindingCount; ++i) {
                if (Bindings[i].Slot == Slot && Bindings[i].Type == Type) return &Bindings[i];
            }
            return nullptr;
        }
    };

    struct D3DPipeline {
        ComPtr<ID3D12PipelineState> PSO;
        LayoutHandle Layout {};
        D3D12_PRIMITIVE_TOPOLOGY Topology {D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST};
        std::array<u16, MAX_VERTEX_BUFFERS> Strides {};
        bool IsCompute {false};
    };

    /// @brief A pool slot's contents, moved out at Destroy*() time and held
    /// here until the fence proves no in-flight frame can still reference it.
    ///
    /// This is what makes Destroy*() safe to call the same frame a resource
    /// was drawn with, per IRenderDevice's documented contract: the pool slot
    /// is freed/recycled immediately (Pool::Free), but the actual COM object -
    /// and therefore the real GPU delete - doesn't happen until FenceValue has
    /// been signaled.
    template<typename T>
    struct RetiredResource {
        u64 FenceValue {0};
        T Item;
    };

    /// @brief Per-frame-in-flight upload-heap ring for AllocateTransient.
    ///
    /// One persistently-mapped upload buffer per frame in flight, bump-
    /// allocated and reset at BeginFrame - the direct D3D12 analogue of the
    /// old GL backend's TransientRing (which mapped a coherent GL buffer the
    /// same way, fenced per frame).
    class TransientRing {
    public:
        bool Initialize(ID3D12Device* Device, D3D12MA::Allocator* Allocator, u32 BytesPerFrame, u32 FramesInFlight);
        void Shutdown();

        void BeginFrame(u32 FrameIndex);

        TransientAllocation Allocate(u32 Size, BufferHandle ArenaHandle);

        NODISCARD BufferHandle GetHandle(const u32 FrameIndex) const { return _Handles[FrameIndex]; }
        void SetHandle(const u32 FrameIndex, const BufferHandle Handle) { _Handles[FrameIndex] = Handle; }
        NODISCARD ID3D12Resource* GetResource(const u32 FrameIndex) const { return _Arenas[FrameIndex].Resource.Get(); }

    private:
        struct Arena {
            ComPtr<ID3D12Resource> Resource;
            ComPtr<D3D12MA::Allocation> Allocation;
            u8* Mapped {nullptr};
        };

        std::vector<Arena> _Arenas;
        std::vector<BufferHandle> _Handles;
        u32 _Capacity {0};
        u32 _Head {0};
        u32 _FrameIndex {0};
        u32 _Align {256};  // D3D12 constant buffer view alignment requirement
    };

    class D3D12RenderDevice final : public IRenderDevice {
    public:
        D3D12RenderDevice();
        ~D3D12RenderDevice() override;

        bool Initialize(const DeviceDescriptor& Desc) override;
        void Shutdown() override;
        void WaitIdle() override {
            WaitForGPUIdle();
            ProcessDeferredDeletes();
        }

        NODISCARD Backend GetBackend() const override { return Backend::D3D12; }
        NODISCARD const DeviceCaps& GetCaps() const override { return _Caps; }

        BufferHandle CreateBuffer(const BufferDesc& Desc) override;
        TextureHandle CreateTexture(const TextureDesc& Desc) override;
        SamplerHandle CreateSampler(const SamplerDesc& Desc) override;
        ShaderHandle CreateShader(const ShaderDesc& Desc) override;
        LayoutHandle CreatePipelineLayout(const PipelineLayoutDesc& Desc) override;
        PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& Desc) override;
        PipelineHandle CreateComputePipeline(const ComputePipelineDesc& Desc) override;

        void DestroyBuffer(BufferHandle Handle) override;
        void DestroyTexture(TextureHandle Handle) override;
        void DestroySampler(SamplerHandle Handle) override;
        void DestroyShader(ShaderHandle Handle) override;
        void DestroyPipelineLayout(LayoutHandle Handle) override;
        void DestroyPipeline(PipelineHandle Handle) override;

        void UploadTexture(TextureHandle Handle, const TextureUploadDesc& Upload) override;
        void UpdateBuffer(BufferHandle Handle, u64 Offset, const void* Data, u64 Size) override;
        void* MapBuffer(BufferHandle Handle, u64 Offset, u64 Size) override;
        void UnmapBuffer(BufferHandle Handle) override;

        void BeginFrame() override;
        void Submit(const CommandBuffer& Commands) override;
        void SubmitAndWait(const CommandBuffer& Commands) override;
        void EndFrame() override;

        void SetSwapChainSize(u32 Width, u32 Height) override;
        NODISCARD u32 GetSwapChainWidth() const override { return _SwapWidth; }
        NODISCARD u32 GetSwapChainHeight() const override { return _SwapHeight; }
        void CopyToSwapChain(TextureHandle Source) override;

        TransientAllocation AllocateTransient(u32 Size, BufferUsage Usage) override;

        NODISCARD const FrameStats& GetLastFrameStats() const override { return _LastStats; }
        NODISCARD const std::vector<GpuScopeTiming>& GetLastFrameGpuTimings() const override {
            return _LastResolvedGpuTimings;
        }
        NODISCARD MemoryStats GetMemoryStats() const override;

        // --- Debug UI (Dear ImGui) integration -----------------------------
        //
        // Narrow, D3D12-specific escape hatch for DebugUI (see DebugUI.hpp) -
        // Dear ImGui's DX12 backend needs the raw device/queue/command list,
        // which the backend-agnostic IRenderDevice interface deliberately
        // never exposes. Consistent with the rest of the engine treating
        // D3D12 as the only target platform rather than pretending at a
        // portability it doesn't have (see Window::GetHandle() returning a
        // raw HWND for the same reason).
        NODISCARD ID3D12Device* GetD3DDevice() const { return _Device.Get(); }
        NODISCARD ID3D12CommandQueue* GetD3DCommandQueue() const { return _Queue.Get(); }
        NODISCARD ID3D12GraphicsCommandList* GetD3DCommandList() const { return _CmdList.Get(); }
        NODISCARD u32 GetFramesInFlight() const { return _FramesInFlight; }
        NODISCARD DXGI_FORMAT GetSwapChainFormat() const { return DXGI_FORMAT_B8G8R8A8_UNORM; }

        /// @brief Binds the current frame's back buffer as the render
        /// target, transitioning PRESENT->RENDER_TARGET if it isn't already
        /// (mirrors ExecuteBeginRenderPass's swap-chain-target path) without
        /// clearing it - for drawing an overlay on top of whatever
        /// CopyToSwapChain/EndRenderPass already put there. Pair with
        /// UnbindSwapChainOverlayTarget, called between CopyToSwapChain and
        /// EndFrame.
        void BindSwapChainOverlayTarget();

        /// @brief Transitions the back buffer RENDER_TARGET->PRESENT. Pairs
        /// with BindSwapChainOverlayTarget.
        void UnbindSwapChainOverlayTarget();

    private:
        void CreateSwapChain();
        void ResizeSwapChain(u32 Width, u32 Height);
        void WaitForFrame(u32 FrameIndex);
        void WaitForGPUIdle();

        /// @brief Releases every retired resource whose tagged fence value the
        /// GPU has already passed. Cheap (a few completed-value checks) so it
        /// runs every BeginFrame; also called explicitly from WaitIdle()/
        /// Shutdown() to force a full flush once the GPU is known idle.
        void ProcessDeferredDeletes();

        /// @brief Reads back FrameIndex's timestamp query results (from
        /// MaxFramesInFlight frames ago, the last time this same ring slot
        /// was used) into _LastResolvedGpuTimings, in milliseconds. Only
        /// safe to call once WaitForFrame(FrameIndex) has returned - that's
        /// the same guarantee that lets _CmdAllocators[FrameIndex] be reset
        /// for reuse, and it's what makes mapping the readback buffer here
        /// safe too (the GPU is done writing it).
        void ResolveGpuTimings(u32 FrameIndex);

        void ExecuteBeginRenderPass(const RenderPassDesc& Desc);
        void ExecuteOffscreenBeginRenderPass(const RenderPassDesc& Desc);
        void ExecuteEndRenderPass();

        /// @brief Records a transition barrier only if Tex isn't already in
        /// NewState, and updates its tracked state either way. Every consumer
        /// of an offscreen texture (render pass attachment, SRV bind) goes
        /// through this instead of assuming a state, so usage order between
        /// passes doesn't matter.
        void TransitionTexture(D3DTexture& Tex, D3D12_RESOURCE_STATES NewState);

        u32 AllocateSrvSlot();
        u32 AllocateSamplerSlot();
        u32 AllocateOffscreenRtvSlot();
        u32 AllocateOffscreenDsvSlot();

        HWND _Hwnd {nullptr};
        DeviceDescriptor _Desc {};
        DeviceCaps _Caps {};

        ComPtr<IDXGIFactory4> _Factory;
        ComPtr<ID3D12Device> _Device;
        ComPtr<D3D12MA::Allocator> _Allocator;
        ComPtr<ID3D12CommandQueue> _Queue;
        ComPtr<IDXGISwapChain3> _SwapChain;

        static constexpr u32 MaxFramesInFlight = 4;
        u32 _FramesInFlight {3};

        std::array<ComPtr<ID3D12CommandAllocator>, MaxFramesInFlight> _CmdAllocators;
        ComPtr<ID3D12GraphicsCommandList> _CmdList;

        // Separate from the per-frame allocator/list above: CreateBuffer (with initial
        // data) and UploadTexture run synchronously and can be called at any time,
        // including between frames while a frame's own command list/allocator might
        // still have GPU work in flight - reusing them here would risk resetting an
        // allocator whose commands haven't finished executing yet.
        ComPtr<ID3D12CommandAllocator> _UploadAllocator;
        ComPtr<ID3D12GraphicsCommandList> _UploadCmdList;
        void ExecuteUploadAndWait(const std::function<void(ID3D12GraphicsCommandList*)>& Record);

        ComPtr<ID3D12Fence> _Fence;
        HANDLE _FenceEvent {nullptr};
        u64 _NextFenceValue {1};
        std::array<u64, MaxFramesInFlight> _FrameFenceValues {};

        ComPtr<ID3D12DescriptorHeap> _RtvHeap;
        u32 _RtvDescriptorSize {0};
        std::array<ComPtr<ID3D12Resource>, MaxFramesInFlight> _BackBuffers;
        std::array<bool, MaxFramesInFlight> _BackBufferIsRenderTarget {};

        static constexpr u32 SrvHeapCapacity     = 4096;
        static constexpr u32 SamplerHeapCapacity  = 64;
        ComPtr<ID3D12DescriptorHeap> _SrvHeap;
        ComPtr<ID3D12DescriptorHeap> _SamplerHeap;
        u32 _SrvDescriptorSize {0};
        u32 _SamplerDescriptorSize {0};
        u32 _NextSrvSlot {0};
        u32 _NextSamplerSlot {0};

        // Populated only once ProcessDeferredDeletes proves the GPU is done
        // with the texture/sampler that owned a slot - a heap slot is a view
        // the GPU can still be reading via an already-recorded command list,
        // exactly like the resource itself, so it can't be recycled any
        // sooner than the resource is (see RetiredResource).
        std::vector<u32> _FreeSrvSlots;
        std::vector<u32> _FreeSamplerSlots;

        // Separate, non-shader-visible heaps for offscreen render-target/
        // depth-stencil views (a Viewport's color target, a post-process
        // scratch texture, ...). Kept apart from _RtvHeap, which is sized
        // exactly to the swap chain's back buffer count and shader-invisible
        // RTV/DSV descriptors don't need to live in a shader-visible heap
        // anyway (only SRV/UAV/sampler descriptors bound via a root
        // descriptor table do).
        // A mip-chained render target takes one slot per mip - and now that
        // TextureCache can give an ordinary material texture ColorTarget
        // usage for GPU mip generation (see MipGenerator), every such
        // texture holds its own slots for its whole resident lifetime (the
        // heap has no way to reclaim them just between uses). A content-
        // heavy scene with many high-resolution textures needs real
        // headroom here; RTV/DSV descriptors are CPU-only and cheap to
        // over-provision (unlike the shader-visible SRV/sampler heaps).
        static constexpr u32 OffscreenRtvHeapCapacity = 8192;
        static constexpr u32 OffscreenDsvHeapCapacity = 8;
        ComPtr<ID3D12DescriptorHeap> _OffscreenRtvHeap;
        ComPtr<ID3D12DescriptorHeap> _OffscreenDsvHeap;
        u32 _OffscreenRtvDescriptorSize {0};
        u32 _OffscreenDsvDescriptorSize {0};
        u32 _NextOffscreenRtvSlot {0};
        u32 _NextOffscreenDsvSlot {0};
        std::vector<u32> _FreeOffscreenRtvSlots;
        std::vector<u32> _FreeOffscreenDsvSlots;

        Pool<D3DBuffer, BufferHandle> _Buffers;
        Pool<D3DTexture, TextureHandle> _Textures;
        Pool<D3DSampler, SamplerHandle> _Samplers;
        Pool<D3DShader, ShaderHandle> _Shaders;
        Pool<D3DPipelineLayout, LayoutHandle> _Layouts;
        Pool<D3DPipeline, PipelineHandle> _Pipelines;

        // Declared after the pools/allocator above (and so destroyed before
        // them, in reverse declaration order) so a still-populated queue at
        // teardown releases its COM objects while the allocator that created
        // them is still alive. Shutdown() flushes these explicitly anyway
        // once the GPU is confirmed idle, so this ordering is a backstop, not
        // the primary mechanism.
        std::deque<RetiredResource<D3DBuffer>> _RetiredBuffers;
        std::deque<RetiredResource<D3DTexture>> _RetiredTextures;
        std::deque<RetiredResource<D3DSampler>> _RetiredSamplers;
        std::deque<RetiredResource<D3DShader>> _RetiredShaders;
        std::deque<RetiredResource<D3DPipelineLayout>> _RetiredLayouts;
        std::deque<RetiredResource<D3DPipeline>> _RetiredPipelines;

        TransientRing _Transient;

        const D3DPipeline* _CurrentPipeline {nullptr};
        const D3DPipelineLayout* _CurrentLayout {nullptr};

        u32 _FrameIndex {0};
        u32 _SwapWidth {0};
        u32 _SwapHeight {0};
        bool _InRenderPass {false};
        bool _SwapChainTargetThisPass {false};

        // Remembered from BeginRenderPass so EndRenderPass can transition an
        // offscreen pass's attachments to a sampling-ready resting state
        // without RenderPassDesc having to be threaded through as well.
        TextureHandle _CurrentColorAttachments[MAX_COLOR_ATTACHMENTS] {};
        u8 _CurrentColorAttachmentCount {0};
        TextureHandle _CurrentDepthAttachment {};
        bool _CurrentHasDepthAttachment {false};

        FrameStats _Stats {};
        FrameStats _LastStats {};

        // --- GPU timing (CommandBuffer::PushDebugGroup/PopDebugGroup) -----
        //
        // One D3D12_QUERY_TYPE_TIMESTAMP per Push and per Pop, all in one
        // heap shared across frames; each frame-in-flight gets its own
        // readback buffer (same "ring indexed by _FrameIndex" shape as
        // _CmdAllocators) since a query's result isn't available until the
        // GPU has actually executed that point in the command stream -
        // BeginFrame's existing WaitForFrame(_FrameIndex) is what makes it
        // safe to read a slot back before reusing it for a new frame (see
        // ResolveGpuTimings).
        static constexpr u32 MaxGpuTimestamps = 128;  // 64 Push/Pop pairs per frame
        ComPtr<ID3D12QueryHeap> _TimestampHeap;
        std::array<ComPtr<ID3D12Resource>, MaxFramesInFlight> _TimestampReadback;
        u64 _TimestampFrequency {0};  // ticks/second - 0 if timestamp queries aren't supported
        u32 _NextTimestampIndex {0};  // reset to 0 every BeginFrame

        struct PendingGpuScope {
            char Name[32] {};
            u32 StartIndex {0};
            u32 EndIndex {0};
            u32 Depth {0};
        };
        // What this frame's Pushes/Pops resolved to (index pairs, not yet
        // turned into milliseconds - that needs the readback buffer, which
        // isn't safe to read until this same ring slot comes back around).
        // Entries land here in Push order, not Pop order: PushDebugGroup
        // appends a placeholder immediately (EndIndex patched in once the
        // matching Pop runs), rather than only recording anything at Pop
        // time - the latter would put a scope's *children* before the scope
        // itself in this list (Pop is innermost-first), which reads
        // backwards in a nested display; Push order is a normal depth-first
        // traversal, parent immediately followed by its own children.
        std::array<std::vector<PendingGpuScope>, MaxFramesInFlight> _PendingGpuScopes;

        // CPU-side stack of not-yet-popped PushDebugGroup calls, mirroring
        // the nesting those calls already imply - just the index into this
        // frame's _PendingGpuScopes where Pop should patch in EndIndex.
        std::vector<u32> _ActiveGpuScopeStack;

        // The most recently resolved frame's results - what
        // GetLastFrameGpuTimings() returns. Always a few frames "behind"
        // whatever's on screen right now; see that method's own comment.
        std::vector<GpuScopeTiming> _LastResolvedGpuTimings;

        bool _Initialized {false};

    };
}  // namespace Xen::RHI::D3D12Backend
