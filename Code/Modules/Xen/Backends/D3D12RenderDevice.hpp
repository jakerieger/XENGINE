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
    };

    struct D3DSampler {
        u32 HeapIndex {UINT32_MAX};
    };

    struct D3DShader {
        ComPtr<ID3DBlob> Bytecode;
        ShaderStage Stage {ShaderStage::Vertex};
    };

    struct D3DPipeline {
        ComPtr<ID3D12PipelineState> PSO;
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
        void CreateSwapChain();
        void ResizeSwapChain(u32 Width, u32 Height);
        void WaitForFrame(u32 FrameIndex);
        void WaitForGPUIdle();

        /// @brief Releases every retired resource whose tagged fence value the
        /// GPU has already passed. Cheap (a few completed-value checks) so it
        /// runs every BeginFrame; also called explicitly from WaitIdle()/
        /// Shutdown() to force a full flush once the GPU is known idle.
        void ProcessDeferredDeletes();

        void ExecuteBeginRenderPass(const RenderPassDesc& Desc);
        void ExecuteEndRenderPass();

        u32 AllocateSrvSlot();
        u32 AllocateSamplerSlot();

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

        ComPtr<ID3D12RootSignature> _RootSignature;

        Pool<D3DBuffer, BufferHandle> _Buffers;
        Pool<D3DTexture, TextureHandle> _Textures;
        Pool<D3DSampler, SamplerHandle> _Samplers;
        Pool<D3DShader, ShaderHandle> _Shaders;
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
        std::deque<RetiredResource<D3DPipeline>> _RetiredPipelines;

        TransientRing _Transient;

        const D3DPipeline* _CurrentPipeline {nullptr};

        u32 _FrameIndex {0};
        u32 _SwapWidth {0};
        u32 _SwapHeight {0};
        bool _InRenderPass {false};
        bool _SwapChainTargetThisPass {false};

        FrameStats _Stats {};
        FrameStats _LastStats {};

        bool _Initialized {false};

        void Log(bool Error, const char* Fmt, ...) const;
    };
}  // namespace Xen::RHI::D3D12Backend
