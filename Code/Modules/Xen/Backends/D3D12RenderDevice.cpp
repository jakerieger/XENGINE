//
// Created by Jake Rieger on 9/15/2026.
//

#include "D3D12RenderDevice.hpp"
#include "../CommandBuffer.hpp"

#include <d3dcompiler.h>

#include <algorithm>
#include <cstdio>
#include <cstdarg>

namespace Xen::RHI::D3D12Backend {
    namespace {
        constexpr u32 D3D12_UPLOAD_ALIGN = 256;  // constant/vertex buffer view alignment

        u64 AlignUp(const u64 Value, const u64 Align) {
            return (Value + Align - 1) & ~(Align - 1);
        }

        D3D12_RESOURCE_STATES RestingBufferState(const BufferUsage Usage) {
            if (Any(Usage & BufferUsage::Index)) return D3D12_RESOURCE_STATE_INDEX_BUFFER;
            return D3D12_RESOURCE_STATE_GENERIC_READ;
        }

        // Retirement queues are pushed to in non-decreasing FenceValue order
        // (destroys happen in temporal order and _NextFenceValue only ever
        // grows), so popping from the front while the fence has passed the
        // tag is enough - no need to scan the whole deque.
        template<typename Deque>
        void ReleaseCompleted(Deque& Q, const u64 CompletedValue) {
            while (!Q.empty() && Q.front().FenceValue <= CompletedValue)
                Q.pop_front();
        }

        // FIFO recycling, matching Pool<T,H>'s own free-slot convention (see
        // RHI.hpp) - a freed index isn't immediately reissued, so a
        // stale-descriptor bug surfaces promptly instead of intermittently.
        u32 AllocateSlot(std::vector<u32>& FreeList, u32& NextSlot, const u32 Capacity) {
            if (!FreeList.empty()) {
                const u32 Slot = FreeList.front();
                FreeList.erase(FreeList.begin());
                return Slot;
            }
            if (NextSlot >= Capacity) return UINT32_MAX;
            return NextSlot++;
        }
    }  // namespace

    // --- TransientRing ---------------------------------------------------

    bool TransientRing::Initialize(ID3D12Device* Device,
                                   D3D12MA::Allocator* Allocator,
                                   const u32 BytesPerFrame,
                                   const u32 FramesInFlight) {
        _Capacity = CAST<u32>(AlignUp(BytesPerFrame, D3D12_UPLOAD_ALIGN));
        _Arenas.resize(FramesInFlight);
        _Handles.resize(FramesInFlight);

        for (u32 i = 0; i < FramesInFlight; ++i) {
            D3D12MA::ALLOCATION_DESC AllocDesc {};
            AllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC ResDesc {};
            ResDesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
            ResDesc.Width            = _Capacity;
            ResDesc.Height           = 1;
            ResDesc.DepthOrArraySize = 1;
            ResDesc.MipLevels        = 1;
            ResDesc.SampleDesc.Count = 1;
            ResDesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            Arena A;
            const HRESULT Hr = Allocator->CreateResource(&AllocDesc,
                                                         &ResDesc,
                                                         D3D12_RESOURCE_STATE_GENERIC_READ,
                                                         nullptr,
                                                         &A.Allocation,
                                                         IID_PPV_ARGS(&A.Resource));
            if (FAILED(Hr)) return false;

            void* Mapped = nullptr;
            if (FAILED(A.Resource->Map(0, nullptr, &Mapped))) return false;
            A.Mapped   = RCAST<u8*>(Mapped);
            _Arenas[i] = std::move(A);
        }

        (void)Device;
        return true;
    }

    void TransientRing::Shutdown() {
        for (Arena& A : _Arenas) {
            if (A.Resource) A.Resource->Unmap(0, nullptr);
        }
        _Arenas.clear();
        _Handles.clear();
    }

    void TransientRing::BeginFrame(const u32 FrameIndex) {
        _FrameIndex = FrameIndex;
        _Head       = 0;
    }

    TransientAllocation TransientRing::Allocate(const u32 Size, const BufferHandle ArenaHandle) {
        const u32 AlignedSize = CAST<u32>(AlignUp(Size, D3D12_UPLOAD_ALIGN));
        if (_Head + AlignedSize > _Capacity) return {};

        const Arena& A = _Arenas[_FrameIndex];

        TransientAllocation Out;
        Out.Buffer = ArenaHandle;
        Out.Offset = _Head;
        Out.Size   = Size;
        Out.Data   = A.Mapped + _Head;

        _Head += AlignedSize;
        return Out;
    }

    // --- D3D12RenderDevice: lifetime --------------------------------------

    D3D12RenderDevice::D3D12RenderDevice() = default;

    D3D12RenderDevice::~D3D12RenderDevice() {
        Shutdown();
    }

    bool D3D12RenderDevice::Initialize(const DeviceDescriptor& Desc) {
        _Desc           = Desc;
        _Hwnd           = RCAST<HWND>(Desc.NativeWindowHandle);
        _FramesInFlight = std::clamp(Desc.FramesInFlight, 2u, MaxFramesInFlight);

        UINT FactoryFlags = 0;
        if (Desc.EnableValidation) {
            ComPtr<ID3D12Debug> DebugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController)))) {
                DebugController->EnableDebugLayer();
                FactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }

        if (FAILED(CreateDXGIFactory2(FactoryFlags, IID_PPV_ARGS(&_Factory)))) {
            LOG_ERR("CreateDXGIFactory2 failed");
            return false;
        }

        ComPtr<IDXGIAdapter1> Adapter;
        for (UINT i = 0; _Factory->EnumAdapters1(i, &Adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 AdapterDesc;
            Adapter->GetDesc1(&AdapterDesc);
            if (AdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            if (SUCCEEDED(D3D12CreateDevice(Adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_Device)))) break;
            Adapter.Reset();
        }
        if (!_Device) {
            LOG_ERR("no D3D12-capable hardware adapter found");
            return false;
        }

        D3D12_COMMAND_QUEUE_DESC QueueDesc {};
        QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (FAILED(_Device->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&_Queue)))) return false;

        D3D12MA::ALLOCATOR_DESC AllocatorDesc {};
        AllocatorDesc.pDevice  = _Device.Get();
        AllocatorDesc.pAdapter = Adapter.Get();
        if (FAILED(D3D12MA::CreateAllocator(&AllocatorDesc, &_Allocator))) return false;

        for (u32 i = 0; i < _FramesInFlight; ++i) {
            if (FAILED(
                  _Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_CmdAllocators[i]))))
                return false;
        }
        if (FAILED(_Device->CreateCommandList(0,
                                              D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              _CmdAllocators[0].Get(),
                                              nullptr,
                                              IID_PPV_ARGS(&_CmdList))))
            return false;
        _CmdList->Close();

        if (FAILED(_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_UploadAllocator))))
            return false;
        if (FAILED(_Device->CreateCommandList(0,
                                              D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              _UploadAllocator.Get(),
                                              nullptr,
                                              IID_PPV_ARGS(&_UploadCmdList))))
            return false;
        _UploadCmdList->Close();

        if (FAILED(_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_Fence)))) return false;
        _FenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!_FenceEvent) return false;

        D3D12_DESCRIPTOR_HEAP_DESC RtvHeapDesc {};
        RtvHeapDesc.NumDescriptors = _FramesInFlight;
        RtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        if (FAILED(_Device->CreateDescriptorHeap(&RtvHeapDesc, IID_PPV_ARGS(&_RtvHeap)))) return false;
        _RtvDescriptorSize = _Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_DESCRIPTOR_HEAP_DESC SrvHeapDesc {};
        SrvHeapDesc.NumDescriptors = SrvHeapCapacity;
        SrvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        SrvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(_Device->CreateDescriptorHeap(&SrvHeapDesc, IID_PPV_ARGS(&_SrvHeap)))) return false;
        _SrvDescriptorSize = _Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        D3D12_DESCRIPTOR_HEAP_DESC SamplerHeapDesc {};
        SamplerHeapDesc.NumDescriptors = SamplerHeapCapacity;
        SamplerHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        SamplerHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(_Device->CreateDescriptorHeap(&SamplerHeapDesc, IID_PPV_ARGS(&_SamplerHeap)))) return false;
        _SamplerDescriptorSize = _Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

        D3D12_DESCRIPTOR_HEAP_DESC OffscreenRtvHeapDesc {};
        OffscreenRtvHeapDesc.NumDescriptors = OffscreenRtvHeapCapacity;
        OffscreenRtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        if (FAILED(_Device->CreateDescriptorHeap(&OffscreenRtvHeapDesc, IID_PPV_ARGS(&_OffscreenRtvHeap))))
            return false;
        _OffscreenRtvDescriptorSize = _Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_DESCRIPTOR_HEAP_DESC OffscreenDsvHeapDesc {};
        OffscreenDsvHeapDesc.NumDescriptors = OffscreenDsvHeapCapacity;
        OffscreenDsvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        if (FAILED(_Device->CreateDescriptorHeap(&OffscreenDsvHeapDesc, IID_PPV_ARGS(&_OffscreenDsvHeap))))
            return false;
        _OffscreenDsvDescriptorSize = _Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        if (!_Transient.Initialize(_Device.Get(), _Allocator.Get(), Desc.TransientBufferSize, _FramesInFlight)) {
            LOG_ERR("transient ring initialization failed");
            return false;
        }
        // Wrap each frame's transient upload arena as a poolable D3DBuffer, exactly the
        // way CreateBuffer would, so BindVertexBuffer/BindUniformBuffer resolve a
        // transient allocation's handle through the same lookup as any other buffer.
        for (u32 i = 0; i < _FramesInFlight; ++i) {
            D3DBuffer Buf;
            Buf.Resource         = _Transient.GetResource(i);
            Buf.Size             = Desc.TransientBufferSize;
            Buf.Memory           = MemoryUsage::CpuToGpu;
            Buf.Transient        = true;
            const BufferHandle H = _Buffers.Allocate(std::move(Buf));
            _Transient.SetHandle(i, H);
        }

        if (_Hwnd) CreateSwapChain();

        _Caps.Vendor                       = "N/A";
        _Caps.Renderer                     = "D3D12";
        _Caps.ApiVersion                   = "12";
        _Caps.MaxColorAttachments          = MAX_COLOR_ATTACHMENTS;
        _Caps.UniformBufferOffsetAlignment = D3D12_UPLOAD_ALIGN;
        _Caps.StorageBufferOffsetAlignment = D3D12_UPLOAD_ALIGN;
        _Caps.MaxAnisotropy                = 16;
        _Caps.SupportsAnisotropy           = true;
        _Caps.SupportsDebugMarkers         = false;

        _Initialized = true;
        return true;
    }

    void D3D12RenderDevice::Shutdown() {
        if (!_Initialized) return;

        WaitForGPUIdle();
        // Every Destroy*() call ever made only moved its resource into a
        // retirement queue; this is what actually lets them go, now that the
        // wait above guarantees the GPU is done with all of them.
        ProcessDeferredDeletes();

        _Transient.Shutdown();

        if (_FenceEvent) {
            CloseHandle(_FenceEvent);
            _FenceEvent = nullptr;
        }

        // Every pooled resource (buffers, textures, samplers, shaders, pipelines) holds
        // its D3D12/D3D12MA objects in ComPtr members, so they release themselves when
        // this device object is destroyed - nothing to do here explicitly.

        _Initialized = false;
    }

    // --- Swap chain --------------------------------------------------------

    void D3D12RenderDevice::CreateSwapChain() {
        RECT ClientRect;
        GetClientRect(_Hwnd, &ClientRect);
        _SwapWidth  = std::max<u32>(CAST<u32>(ClientRect.right - ClientRect.left), 1);
        _SwapHeight = std::max<u32>(CAST<u32>(ClientRect.bottom - ClientRect.top), 1);

        DXGI_SWAP_CHAIN_DESC1 SwapDesc {};
        SwapDesc.BufferCount      = _FramesInFlight;
        SwapDesc.Width            = _SwapWidth;
        SwapDesc.Height           = _SwapHeight;
        SwapDesc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
        SwapDesc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        SwapDesc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        SwapDesc.SampleDesc.Count = 1;

        ComPtr<IDXGISwapChain1> SwapChain1;
        Xen::NoValue = _Factory->CreateSwapChainForHwnd(_Queue.Get(), _Hwnd, &SwapDesc, nullptr, nullptr, &SwapChain1);
        Xen::NoValue = _Factory->MakeWindowAssociation(_Hwnd, DXGI_MWA_NO_ALT_ENTER);
        Xen::NoValue = SwapChain1.As(&_SwapChain);

        D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle = _RtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (u32 i = 0; i < _FramesInFlight; ++i) {
            _SwapChain->GetBuffer(i, IID_PPV_ARGS(&_BackBuffers[i]));
            _Device->CreateRenderTargetView(_BackBuffers[i].Get(), nullptr, RtvHandle);
            _BackBufferIsRenderTarget[i] = false;
            RtvHandle.ptr += _RtvDescriptorSize;
        }

        _FrameIndex = _SwapChain->GetCurrentBackBufferIndex();
    }

    void D3D12RenderDevice::ResizeSwapChain(const u32 Width, const u32 Height) {
        if (Width == 0 || Height == 0) return;

        WaitForGPUIdle();

        for (auto& BB : _BackBuffers)
            BB.Reset();

        DXGI_SWAP_CHAIN_DESC Desc;
        _SwapChain->GetDesc(&Desc);
        _SwapChain->ResizeBuffers(_FramesInFlight, Width, Height, Desc.BufferDesc.Format, Desc.Flags);

        D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle = _RtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (u32 i = 0; i < _FramesInFlight; ++i) {
            _SwapChain->GetBuffer(i, IID_PPV_ARGS(&_BackBuffers[i]));
            _Device->CreateRenderTargetView(_BackBuffers[i].Get(), nullptr, RtvHandle);
            _BackBufferIsRenderTarget[i] = false;
            RtvHandle.ptr += _RtvDescriptorSize;
        }

        _SwapWidth  = Width;
        _SwapHeight = Height;
        _FrameIndex = _SwapChain->GetCurrentBackBufferIndex();
    }

    void D3D12RenderDevice::SetSwapChainSize(const u32 Width, const u32 Height) {
        if (!_SwapChain) {
            _SwapWidth  = Width;
            _SwapHeight = Height;
            return;
        }
        if (Width == _SwapWidth && Height == _SwapHeight) return;
        ResizeSwapChain(Width, Height);
    }

    void D3D12RenderDevice::CopyToSwapChain(const TextureHandle Source) {
        if (!_SwapChain) return;

        D3DTexture* Tex = _Textures.Get(Source);
        if (!Tex) {
            LOG_ERR("CopyToSwapChain: invalid source texture");
            return;
        }

        // Tracked independently of TransitionTexture (which only knows about
        // pooled textures): the back buffer isn't one, so its state has to be
        // read from the same bookkeeping ExecuteBeginRenderPass/EndRenderPass
        // use for the swap-chain-target path.
        const D3D12_RESOURCE_STATES BackBufferStateBefore =
          _BackBufferIsRenderTarget[_FrameIndex] ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_PRESENT;

        D3D12_RESOURCE_BARRIER ToCopyDest {};
        ToCopyDest.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        ToCopyDest.Transition.pResource   = _BackBuffers[_FrameIndex].Get();
        ToCopyDest.Transition.StateBefore = BackBufferStateBefore;
        ToCopyDest.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
        ToCopyDest.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _CmdList->ResourceBarrier(1, &ToCopyDest);
        _BackBufferIsRenderTarget[_FrameIndex] = false;

        TransitionTexture(*Tex, D3D12_RESOURCE_STATE_COPY_SOURCE);

        _CmdList->CopyResource(_BackBuffers[_FrameIndex].Get(), Tex->Resource.Get());

        D3D12_RESOURCE_BARRIER ToPresent {};
        ToPresent.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        ToPresent.Transition.pResource   = _BackBuffers[_FrameIndex].Get();
        ToPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        ToPresent.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
        ToPresent.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _CmdList->ResourceBarrier(1, &ToPresent);
    }

    // --- Sync ----------------------------------------------------------------

    void D3D12RenderDevice::WaitForFrame(const u32 FrameIndex) {
        const u64 Target = _FrameFenceValues[FrameIndex];
        if (Target == 0) return;  // never submitted yet
        if (_Fence->GetCompletedValue() < Target) {
            _Fence->SetEventOnCompletion(Target, _FenceEvent);
            WaitForSingleObject(_FenceEvent, INFINITE);
        }
    }

    void D3D12RenderDevice::ExecuteUploadAndWait(const std::function<void(ID3D12GraphicsCommandList*)>& Record) {
        _UploadAllocator->Reset();
        _UploadCmdList->Reset(_UploadAllocator.Get(), nullptr);

        Record(_UploadCmdList.Get());

        _UploadCmdList->Close();
        ID3D12CommandList* Lists[] = {_UploadCmdList.Get()};
        _Queue->ExecuteCommandLists(1, Lists);
        WaitForGPUIdle();
    }

    void D3D12RenderDevice::WaitForGPUIdle() {
        if (!_Queue || !_Fence) return;
        const u64 Value = _NextFenceValue++;
        _Queue->Signal(_Fence.Get(), Value);
        if (_Fence->GetCompletedValue() < Value) {
            _Fence->SetEventOnCompletion(Value, _FenceEvent);
            WaitForSingleObject(_FenceEvent, INFINITE);
        }
    }

    void D3D12RenderDevice::ProcessDeferredDeletes() {
        if (!_Fence) return;
        const u64 Completed = _Fence->GetCompletedValue();

        ReleaseCompleted(_RetiredBuffers, Completed);

        // A heap slot is a view the GPU can still be reading via an
        // already-recorded command list, exactly like the resource it views -
        // it only goes back on its free list here, once the fence proves the
        // GPU is done with the resource that owned it (see RetiredResource).
        while (!_RetiredTextures.empty() && _RetiredTextures.front().FenceValue <= Completed) {
            const D3DTexture& Tex = _RetiredTextures.front().Item;
            if (Tex.SrvHeapIndex != UINT32_MAX) _FreeSrvSlots.push_back(Tex.SrvHeapIndex);
            if (Tex.RtvHeapIndex != UINT32_MAX) _FreeOffscreenRtvSlots.push_back(Tex.RtvHeapIndex);
            if (Tex.DsvHeapIndex != UINT32_MAX) _FreeOffscreenDsvSlots.push_back(Tex.DsvHeapIndex);
            _RetiredTextures.pop_front();
        }

        while (!_RetiredSamplers.empty() && _RetiredSamplers.front().FenceValue <= Completed) {
            const D3DSampler& Samp = _RetiredSamplers.front().Item;
            if (Samp.HeapIndex != UINT32_MAX) _FreeSamplerSlots.push_back(Samp.HeapIndex);
            _RetiredSamplers.pop_front();
        }

        ReleaseCompleted(_RetiredShaders, Completed);
        ReleaseCompleted(_RetiredLayouts, Completed);
        ReleaseCompleted(_RetiredPipelines, Completed);
    }

    // --- Frame -----------------------------------------------------------

    void D3D12RenderDevice::BeginFrame() {
        _Stats = {};

        ProcessDeferredDeletes();

        if (_SwapChain) _FrameIndex = _SwapChain->GetCurrentBackBufferIndex();
        WaitForFrame(_FrameIndex);

        _CmdAllocators[_FrameIndex]->Reset();
        _CmdList->Reset(_CmdAllocators[_FrameIndex].Get(), nullptr);

        ID3D12DescriptorHeap* Heaps[] = {_SrvHeap.Get(), _SamplerHeap.Get()};
        _CmdList->SetDescriptorHeaps(2, Heaps);

        _Transient.BeginFrame(_FrameIndex);
        _CurrentPipeline = nullptr;
        _CurrentLayout   = nullptr;
    }

    void D3D12RenderDevice::Submit(const CommandBuffer& Commands) {
        CommandIterator It(Commands);

        while (It.HasNext()) {
            const CmdHeader& Header = It.Next();

            switch (Header.Type) {
                case CmdType::BeginRenderPass:
                    ExecuteBeginRenderPass(It.Payload<Cmd::BeginRenderPass>().Desc);
                    break;
                case CmdType::EndRenderPass:
                    ExecuteEndRenderPass();
                    break;

                case CmdType::SetViewport: {
                    const auto& P = It.Payload<Cmd::SetViewport>();
                    const D3D12_VIEWPORT VP {P.View.X,
                                             P.View.Y,
                                             P.View.Width,
                                             P.View.Height,
                                             P.View.MinDepth,
                                             P.View.MaxDepth};
                    _CmdList->RSSetViewports(1, &VP);
                    break;
                }

                case CmdType::SetScissor: {
                    const auto& P = It.Payload<Cmd::SetScissor>();
                    if (P.Enabled) {
                        const D3D12_RECT R {P.Rect.X,
                                            P.Rect.Y,
                                            P.Rect.X + CAST<LONG>(P.Rect.Width),
                                            P.Rect.Y + CAST<LONG>(P.Rect.Height)};
                        _CmdList->RSSetScissorRects(1, &R);
                    } else {
                        const D3D12_RECT R {0, 0, CAST<LONG>(_SwapWidth), CAST<LONG>(_SwapHeight)};
                        _CmdList->RSSetScissorRects(1, &R);
                    }
                    break;
                }

                case CmdType::SetBlendConstants: {
                    const auto& P = It.Payload<Cmd::SetBlendConstants>();
                    _CmdList->OMSetBlendFactor(P.Constants);
                    break;
                }

                case CmdType::SetStencilReference:
                    _CmdList->OMSetStencilRef(It.Payload<Cmd::SetStencilReference>().Reference);
                    break;

                case CmdType::BindPipeline: {
                    const auto& P    = It.Payload<Cmd::BindPipeline>();
                    _CurrentPipeline = _Pipelines.Get(P.Pipeline);
                    if (_CurrentPipeline) {
                        // Compared by pointer, not handle: two pipelines that
                        // share a layout (common - many material variants
                        // reuse one binding shape) correctly skip the
                        // redundant root signature set.
                        if (const D3DPipelineLayout* NewLayout = _Layouts.Get(_CurrentPipeline->Layout);
                            NewLayout != _CurrentLayout) {
                            _CurrentLayout = NewLayout;
                            if (_CurrentLayout) {
                                if (_CurrentPipeline->IsCompute)
                                    _CmdList->SetComputeRootSignature(_CurrentLayout->RootSignature.Get());
                                else _CmdList->SetGraphicsRootSignature(_CurrentLayout->RootSignature.Get());
                            }
                        }

                        _CmdList->SetPipelineState(_CurrentPipeline->PSO.Get());
                        if (!_CurrentPipeline->IsCompute) _CmdList->IASetPrimitiveTopology(_CurrentPipeline->Topology);
                    }
                    ++_Stats.PipelineBinds;
                    break;
                }

                case CmdType::BindVertexBuffer: {
                    const auto& P      = It.Payload<Cmd::BindVertexBuffer>();
                    const D3DBuffer* B = _Buffers.Get(P.Buffer);
                    if (B && _CurrentPipeline) {
                        D3D12_VERTEX_BUFFER_VIEW View {};
                        View.BufferLocation = B->Resource->GetGPUVirtualAddress() + P.Offset;
                        View.SizeInBytes    = CAST<UINT>(B->Size - P.Offset);
                        View.StrideInBytes  = _CurrentPipeline->Strides[P.Slot];
                        _CmdList->IASetVertexBuffers(P.Slot, 1, &View);
                    }
                    break;
                }

                case CmdType::BindIndexBuffer: {
                    const auto& P      = It.Payload<Cmd::BindIndexBuffer>();
                    const D3DBuffer* B = _Buffers.Get(P.Buffer);
                    if (B) {
                        D3D12_INDEX_BUFFER_VIEW View {};
                        View.BufferLocation = B->Resource->GetGPUVirtualAddress() + P.Offset;
                        View.SizeInBytes    = CAST<UINT>(B->Size - P.Offset);
                        View.Format         = P.Type == IndexType::U16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
                        _CmdList->IASetIndexBuffer(&View);
                    }
                    break;
                }

                case CmdType::BindUniformBuffer: {
                    const auto& P      = It.Payload<Cmd::BindUniformBuffer>();
                    const D3DBuffer* B = _Buffers.Get(P.Buffer);
                    if (B && _CurrentLayout && _CurrentPipeline) {
                        // A table-bound (array) uniform buffer isn't exercised
                        // yet - would need a CBV descriptor written into the
                        // heap first, not just a GPU virtual address.
                        if (const auto* Binding = _CurrentLayout->Find(P.Slot, BindingType::UniformBuffer);
                            Binding && !Binding->IsTable) {
                            const D3D12_GPU_VIRTUAL_ADDRESS Address = B->Resource->GetGPUVirtualAddress() + P.Offset;
                            if (_CurrentPipeline->IsCompute)
                                _CmdList->SetComputeRootConstantBufferView(Binding->RootParameterIndex, Address);
                            else _CmdList->SetGraphicsRootConstantBufferView(Binding->RootParameterIndex, Address);
                        }
                    }
                    break;
                }

                case CmdType::BindStorageBuffer:
                    // Not exercised by any pipeline yet (no UAV buffer view
                    // creation path) - left unimplemented, same as before the
                    // pipeline layout generalization.
                    break;

                case CmdType::BindTexture: {
                    const auto& P = It.Payload<Cmd::BindTexture>();
                    if (!_CurrentLayout || !_CurrentPipeline) break;

                    // A texture and the sampler it's bound with share one
                    // Slot value across their separate register spaces (t#
                    // and s#) - the same convention the HLSL itself uses
                    // (Texture2D : register(t0); SamplerState : register(s0)),
                    // so one BindTexture call can resolve both.
                    if (D3DTexture* Tex = _Textures.Get(P.Texture); Tex && Tex->SrvHeapIndex != UINT32_MAX) {
                        // Self-transitioning: a texture that was just an
                        // offscreen render target (or was never rendered into
                        // at all yet) may not be SRV-readable already.
                        TransitionTexture(
                          *Tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                        if (const auto* Binding = _CurrentLayout->Find(P.Slot, BindingType::SampledTexture)) {
                            D3D12_GPU_DESCRIPTOR_HANDLE Handle = _SrvHeap->GetGPUDescriptorHandleForHeapStart();
                            Handle.ptr += CAST<UINT64>(Tex->SrvHeapIndex) * _SrvDescriptorSize;
                            if (_CurrentPipeline->IsCompute)
                                _CmdList->SetComputeRootDescriptorTable(Binding->RootParameterIndex, Handle);
                            else _CmdList->SetGraphicsRootDescriptorTable(Binding->RootParameterIndex, Handle);
                        }
                    }
                    if (const D3DSampler* Samp = _Samplers.Get(P.Sampler); Samp && Samp->HeapIndex != UINT32_MAX) {
                        if (const auto* Binding = _CurrentLayout->Find(P.Slot, BindingType::Sampler)) {
                            D3D12_GPU_DESCRIPTOR_HANDLE Handle = _SamplerHeap->GetGPUDescriptorHandleForHeapStart();
                            Handle.ptr += CAST<UINT64>(Samp->HeapIndex) * _SamplerDescriptorSize;
                            if (_CurrentPipeline->IsCompute)
                                _CmdList->SetComputeRootDescriptorTable(Binding->RootParameterIndex, Handle);
                            else _CmdList->SetGraphicsRootDescriptorTable(Binding->RootParameterIndex, Handle);
                        }
                    }
                    break;
                }

                case CmdType::Draw: {
                    const auto& P = It.Payload<Cmd::Draw>();
                    _CmdList->DrawInstanced(P.VertexCount, P.InstanceCount, P.FirstVertex, P.FirstInstance);
                    ++_Stats.DrawCalls;
                    break;
                }

                case CmdType::DrawIndexed: {
                    const auto& P = It.Payload<Cmd::DrawIndexed>();
                    _CmdList->DrawIndexedInstanced(P.IndexCount,
                                                   P.InstanceCount,
                                                   P.FirstIndex,
                                                   P.VertexOffset,
                                                   P.FirstInstance);
                    ++_Stats.DrawCalls;
                    break;
                }

                case CmdType::Dispatch: {
                    const auto& P = It.Payload<Cmd::Dispatch>();
                    _CmdList->Dispatch(P.GroupsX, P.GroupsY, P.GroupsZ);
                    break;
                }

                // Not exercised by Stage 1's sprite-only workload; every DEFAULT-heap
                // buffer settles into one resting state at creation and is never
                // revisited (see RestingBufferState), which is what lets the general
                // barrier tracking these three opcodes would otherwise need be skipped.
                case CmdType::DrawIndexedIndirect:
                case CmdType::MultiDrawIndexedIndirect:
                case CmdType::UpdateBuffer:
                case CmdType::CopyBuffer:
                case CmdType::GenerateMips:
                case CmdType::PipelineBarrier:
                case CmdType::PushDebugGroup:
                case CmdType::PopDebugGroup:
                case CmdType::InsertDebugMarker:
                default:
                    break;
            }

            ++_Stats.CommandsExecuted;
        }
    }

    void D3D12RenderDevice::EndFrame() {
        _CmdList->Close();
        ID3D12CommandList* Lists[] = {_CmdList.Get()};
        _Queue->ExecuteCommandLists(1, Lists);

        if (_SwapChain) _SwapChain->Present(1, 0);

        const u64 Value                = _NextFenceValue++;
        _FrameFenceValues[_FrameIndex] = Value;
        _Queue->Signal(_Fence.Get(), Value);

        _LastStats = _Stats;
    }

    void D3D12RenderDevice::ExecuteBeginRenderPass(const RenderPassDesc& Desc) {
        _InRenderPass            = true;
        _SwapChainTargetThisPass = Desc.IsSwapChainTarget;

        if (!Desc.IsSwapChainTarget) {
            ExecuteOffscreenBeginRenderPass(Desc);
            return;
        }

        if (!_BackBufferIsRenderTarget[_FrameIndex]) {
            D3D12_RESOURCE_BARRIER Barrier {};
            Barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            Barrier.Transition.pResource   = _BackBuffers[_FrameIndex].Get();
            Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            Barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
            Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            _CmdList->ResourceBarrier(1, &Barrier);
            _BackBufferIsRenderTarget[_FrameIndex] = true;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE Rtv = _RtvHeap->GetCPUDescriptorHandleForHeapStart();
        Rtv.ptr += CAST<UINT64>(_FrameIndex) * _RtvDescriptorSize;
        _CmdList->OMSetRenderTargets(1, &Rtv, FALSE, nullptr);

        if (Desc.ColorAttachmentCount > 0 && Desc.ColorAttachments[0].Load == LoadOp::Clear) {
            _CmdList->ClearRenderTargetView(Rtv, Desc.ColorAttachments[0].Clear.Color, 0, nullptr);
        }

        // No explicit SetViewport/SetScissor command is guaranteed before the first
        // draw (the sprite renderer never issues one), so default both to the full
        // swap chain - a later explicit command in the stream still overrides this.
        const D3D12_VIEWPORT VP {0.0f, 0.0f, CAST<f32>(_SwapWidth), CAST<f32>(_SwapHeight), 0.0f, 1.0f};
        _CmdList->RSSetViewports(1, &VP);
        const D3D12_RECT Scissor {0, 0, CAST<LONG>(_SwapWidth), CAST<LONG>(_SwapHeight)};
        _CmdList->RSSetScissorRects(1, &Scissor);

        ++_Stats.RenderPasses;
    }

    void D3D12RenderDevice::ExecuteOffscreenBeginRenderPass(const RenderPassDesc& Desc) {
        D3D12_CPU_DESCRIPTOR_HANDLE RtvHandles[MAX_COLOR_ATTACHMENTS] {};
        u32 Width = 0, Height = 0;

        _CurrentColorAttachmentCount = 0;
        _CurrentHasDepthAttachment   = false;

        for (u8 i = 0; i < Desc.ColorAttachmentCount; ++i) {
            D3DTexture* Tex = _Textures.Get(Desc.ColorAttachments[i].Texture);
            if (!Tex || Tex->RtvHeapIndex == UINT32_MAX) {
                LOG_ERR("offscreen render pass: color attachment %u is not a valid render-target texture", i);
                continue;
            }

            TransitionTexture(*Tex, D3D12_RESOURCE_STATE_RENDER_TARGET);

            D3D12_CPU_DESCRIPTOR_HANDLE Handle = _OffscreenRtvHeap->GetCPUDescriptorHandleForHeapStart();
            Handle.ptr += CAST<UINT64>(Tex->RtvHeapIndex) * _OffscreenRtvDescriptorSize;
            RtvHandles[i] = Handle;

            if (Desc.ColorAttachments[i].Load == LoadOp::Clear) {
                _CmdList->ClearRenderTargetView(Handle, Desc.ColorAttachments[i].Clear.Color, 0, nullptr);
            }

            Width  = Tex->Width;
            Height = Tex->Height;

            _CurrentColorAttachments[_CurrentColorAttachmentCount++] = Desc.ColorAttachments[i].Texture;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE DsvHandle {};
        bool HasDsv = false;

        if (Desc.HasDepthStencil) {
            D3DTexture* DepthTex = _Textures.Get(Desc.DepthStencil.Texture);
            if (DepthTex && DepthTex->DsvHeapIndex != UINT32_MAX) {
                TransitionTexture(*DepthTex, D3D12_RESOURCE_STATE_DEPTH_WRITE);

                DsvHandle = _OffscreenDsvHeap->GetCPUDescriptorHandleForHeapStart();
                DsvHandle.ptr += CAST<UINT64>(DepthTex->DsvHeapIndex) * _OffscreenDsvDescriptorSize;
                HasDsv = true;

                u32 ClearFlagsRaw = 0;
                if (Desc.DepthStencil.DepthLoad == LoadOp::Clear) ClearFlagsRaw |= D3D12_CLEAR_FLAG_DEPTH;
                if (Desc.DepthStencil.StencilLoad == LoadOp::Clear) ClearFlagsRaw |= D3D12_CLEAR_FLAG_STENCIL;
                if (ClearFlagsRaw != 0) {
                    _CmdList->ClearDepthStencilView(DsvHandle,
                                                    CAST<D3D12_CLEAR_FLAGS>(ClearFlagsRaw),
                                                    Desc.DepthStencil.Clear.Depth,
                                                    CAST<UINT8>(Desc.DepthStencil.Clear.Stencil),
                                                    0,
                                                    nullptr);
                }

                if (Width == 0) {
                    Width  = DepthTex->Width;
                    Height = DepthTex->Height;
                }

                _CurrentDepthAttachment    = Desc.DepthStencil.Texture;
                _CurrentHasDepthAttachment = true;
            } else {
                LOG_ERR("offscreen render pass: depth attachment is not a valid depth-target texture");
            }
        }

        _CmdList->OMSetRenderTargets(Desc.ColorAttachmentCount, RtvHandles, FALSE, HasDsv ? &DsvHandle : nullptr);

        const D3D12_VIEWPORT VP {0.0f, 0.0f, CAST<f32>(Width), CAST<f32>(Height), 0.0f, 1.0f};
        _CmdList->RSSetViewports(1, &VP);
        const D3D12_RECT Scissor {0, 0, CAST<LONG>(Width), CAST<LONG>(Height)};
        _CmdList->RSSetScissorRects(1, &Scissor);

        ++_Stats.RenderPasses;
    }

    void D3D12RenderDevice::ExecuteEndRenderPass() {
        if (!_SwapChainTargetThisPass) {
            // Left sampling-ready (both pixel- and compute-shader-readable) so
            // whatever consumes this pass's output next - a post-process
            // dispatch, a fullscreen blit, ImGui::Image - needs no barrier of
            // its own.
            constexpr D3D12_RESOURCE_STATES ShaderReadable =
              D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

            for (u8 i = 0; i < _CurrentColorAttachmentCount; ++i) {
                if (D3DTexture* Tex = _Textures.Get(_CurrentColorAttachments[i])) TransitionTexture(*Tex, ShaderReadable);
            }

            if (_CurrentHasDepthAttachment) {
                if (D3DTexture* DepthTex = _Textures.Get(_CurrentDepthAttachment)) {
                    // Only a depth texture created with Sampled usage has an
                    // SRV over its typeless resource (see CreateTexture) - one
                    // that's render-only has nothing valid to read it as, so
                    // it stays DEPTH_WRITE, ready for the next pass to write
                    // into again.
                    if (Any(DepthTex->Usage & TextureUsage::Sampled)) TransitionTexture(*DepthTex, ShaderReadable);
                }
            }

            _CurrentColorAttachmentCount = 0;
            _CurrentHasDepthAttachment   = false;
            _InRenderPass                = false;
            return;
        }

        if (_SwapChainTargetThisPass && _BackBufferIsRenderTarget[_FrameIndex]) {
            D3D12_RESOURCE_BARRIER Barrier {};
            Barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            Barrier.Transition.pResource   = _BackBuffers[_FrameIndex].Get();
            Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            Barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
            Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            _CmdList->ResourceBarrier(1, &Barrier);
            _BackBufferIsRenderTarget[_FrameIndex] = false;
        }

        _InRenderPass = false;
    }

    // --- Resources: buffers ------------------------------------------------

    BufferHandle D3D12RenderDevice::CreateBuffer(const BufferDesc& Desc) {
        D3DBuffer Buf;
        Buf.Size   = Desc.Size;
        Buf.Usage  = Desc.Usage;
        Buf.Memory = Desc.Memory;

        D3D12MA::ALLOCATION_DESC AllocDesc {};
        AllocDesc.HeapType = Desc.Memory == MemoryUsage::CpuToGpu   ? D3D12_HEAP_TYPE_UPLOAD
                             : Desc.Memory == MemoryUsage::GpuToCpu ? D3D12_HEAP_TYPE_READBACK
                                                                    : D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC ResDesc {};
        ResDesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        ResDesc.Width            = std::max<u64>(Desc.Size, 1);
        ResDesc.Height           = 1;
        ResDesc.DepthOrArraySize = 1;
        ResDesc.MipLevels        = 1;
        ResDesc.SampleDesc.Count = 1;
        ResDesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        const bool HasInitial                 = Desc.InitialData != nullptr && Desc.Size > 0;
        const D3D12_RESOURCE_STATES RestState = RestingBufferState(Desc.Usage);

        D3D12_RESOURCE_STATES InitialState = RestState;
        if (AllocDesc.HeapType == D3D12_HEAP_TYPE_UPLOAD) InitialState = D3D12_RESOURCE_STATE_GENERIC_READ;
        else if (AllocDesc.HeapType == D3D12_HEAP_TYPE_READBACK) InitialState = D3D12_RESOURCE_STATE_COPY_DEST;
        else if (HasInitial) InitialState = D3D12_RESOURCE_STATE_COPY_DEST;

        const HRESULT Hr = _Allocator->CreateResource(&AllocDesc,
                                                      &ResDesc,
                                                      InitialState,
                                                      nullptr,
                                                      &Buf.Allocation,
                                                      IID_PPV_ARGS(&Buf.Resource));
        if (FAILED(Hr)) return {};

        if (AllocDesc.HeapType == D3D12_HEAP_TYPE_UPLOAD) {
            Buf.Resource->Map(0, nullptr, &Buf.Mapped);
            if (HasInitial) std::memcpy(Buf.Mapped, Desc.InitialData, Desc.Size);
        } else if (AllocDesc.HeapType == D3D12_HEAP_TYPE_DEFAULT && HasInitial) {
            D3D12MA::ALLOCATION_DESC UploadDesc {};
            UploadDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC UploadResDesc = ResDesc;

            ComPtr<D3D12MA::Allocation> UploadAlloc;
            ComPtr<ID3D12Resource> UploadRes;
            if (SUCCEEDED(_Allocator->CreateResource(&UploadDesc,
                                                     &UploadResDesc,
                                                     D3D12_RESOURCE_STATE_GENERIC_READ,
                                                     nullptr,
                                                     &UploadAlloc,
                                                     IID_PPV_ARGS(&UploadRes)))) {
                void* Mapped = nullptr;
                UploadRes->Map(0, nullptr, &Mapped);
                std::memcpy(Mapped, Desc.InitialData, Desc.Size);
                UploadRes->Unmap(0, nullptr);

                ExecuteUploadAndWait([&](ID3D12GraphicsCommandList* List) {
                    List->CopyBufferRegion(Buf.Resource.Get(), 0, UploadRes.Get(), 0, Desc.Size);

                    D3D12_RESOURCE_BARRIER Barrier {};
                    Barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    Barrier.Transition.pResource   = Buf.Resource.Get();
                    Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                    Barrier.Transition.StateAfter  = RestState;
                    Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                    List->ResourceBarrier(1, &Barrier);
                });
            }
        }

        return _Buffers.Allocate(std::move(Buf));
    }

    void D3D12RenderDevice::DestroyBuffer(const BufferHandle Handle) {
        D3DBuffer* B = _Buffers.Get(Handle);
        if (B) {
            if (B->Transient) return;  // owned by the ring, not by us - never freed from the pool either
            _RetiredBuffers.push_back({_NextFenceValue, std::move(*B)});
        }
        _Buffers.Free(Handle);
    }

    void D3D12RenderDevice::UpdateBuffer(BufferHandle, u64, const void*, u64) {
        LOG_ERR("UpdateBuffer is not implemented - not exercised by the sprite pipeline");
    }

    void* D3D12RenderDevice::MapBuffer(const BufferHandle Handle, const u64 Offset, u64) {
        D3DBuffer* B = _Buffers.Get(Handle);
        if (!B || !B->Mapped) return nullptr;
        return RCAST<u8*>(B->Mapped) + Offset;
    }

    void D3D12RenderDevice::UnmapBuffer(BufferHandle) {
        // Buffers stay persistently mapped for their whole lifetime (see CreateBuffer) -
        // nothing to do.
    }

    // --- Resources: textures -------------------------------------------------

    u32 D3D12RenderDevice::AllocateSrvSlot() {
        return AllocateSlot(_FreeSrvSlots, _NextSrvSlot, SrvHeapCapacity);
    }

    u32 D3D12RenderDevice::AllocateSamplerSlot() {
        return AllocateSlot(_FreeSamplerSlots, _NextSamplerSlot, SamplerHeapCapacity);
    }

    u32 D3D12RenderDevice::AllocateOffscreenRtvSlot() {
        return AllocateSlot(_FreeOffscreenRtvSlots, _NextOffscreenRtvSlot, OffscreenRtvHeapCapacity);
    }

    u32 D3D12RenderDevice::AllocateOffscreenDsvSlot() {
        return AllocateSlot(_FreeOffscreenDsvSlots, _NextOffscreenDsvSlot, OffscreenDsvHeapCapacity);
    }

    void D3D12RenderDevice::TransitionTexture(D3DTexture& Tex, const D3D12_RESOURCE_STATES NewState) {
        if (Tex.CurrentState == NewState) return;

        D3D12_RESOURCE_BARRIER Barrier {};
        Barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        Barrier.Transition.pResource   = Tex.Resource.Get();
        Barrier.Transition.StateBefore = Tex.CurrentState;
        Barrier.Transition.StateAfter  = NewState;
        Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _CmdList->ResourceBarrier(1, &Barrier);

        Tex.CurrentState = NewState;
    }

    TextureHandle D3D12RenderDevice::CreateTexture(const TextureDesc& Desc) {
        D3DTexture Tex;
        Tex.Format    = ToDXGIFormat(Desc.Fmt);
        Tex.Width     = Desc.Width;
        Tex.Height    = Desc.Height;
        Tex.MipLevels = Desc.MipLevels == 0 ? 0 : Desc.MipLevels;
        Tex.Usage     = Desc.Usage;

        D3D12MA::ALLOCATION_DESC AllocDesc {};
        AllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        const bool WantsColorTarget = Any(Desc.Usage & TextureUsage::ColorTarget);
        const bool WantsDepthTarget = Any(Desc.Usage & TextureUsage::DepthTarget);
        const bool WantsSampled     = Any(Desc.Usage & TextureUsage::Sampled);

        // A depth texture that's only ever a render target can be created
        // directly in its DSV format. One that's also sampled (a Viewport's
        // depth buffer read back for soft particles, SSAO, ...) can't: a DSV
        // and an SRV over the same memory can't both use a depth-typed
        // format, so the resource itself must be typeless, with the DSV and
        // SRV each applying their own compatible format over it.
        const DXGI_FORMAT ResourceFormat = (WantsDepthTarget && WantsSampled) ? ToTypelessDepthFormat(Desc.Fmt)
                                                                               : Tex.Format;

        D3D12_RESOURCE_DESC ResDesc {};
        ResDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        ResDesc.Width            = Desc.Width;
        ResDesc.Height           = Desc.Height;
        ResDesc.DepthOrArraySize = CAST<UINT16>(std::max<u32>(Desc.ArrayLayers, 1));
        ResDesc.MipLevels        = CAST<UINT16>(Tex.MipLevels == 0 ? 1 : Tex.MipLevels);
        ResDesc.Format           = ResourceFormat;
        ResDesc.SampleDesc.Count = std::max<u32>(Desc.SampleCount, 1);
        ResDesc.Flags            = WantsColorTarget   ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
                                    : WantsDepthTarget ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
                                                        : D3D12_RESOURCE_FLAG_NONE;

        // A plain sampled-only texture (e.g. a sprite) is created COMMON and
        // transitioned explicitly by UploadTexture; a render/depth target is
        // created directly in the state it'll actually be used in, since
        // nothing else transitions it before the first render pass that
        // targets it.
        const D3D12_RESOURCE_STATES InitialState = WantsColorTarget   ? D3D12_RESOURCE_STATE_RENDER_TARGET
                                                    : WantsDepthTarget ? D3D12_RESOURCE_STATE_DEPTH_WRITE
                                                                       : D3D12_RESOURCE_STATE_COMMON;

        const HRESULT Hr = _Allocator->CreateResource(
          &AllocDesc, &ResDesc, InitialState, nullptr, &Tex.Allocation, IID_PPV_ARGS(&Tex.Resource));
        if (FAILED(Hr)) return {};

        Tex.CurrentState = InitialState;

        if (WantsSampled) {
            Tex.SrvHeapIndex = AllocateSrvSlot();
            if (Tex.SrvHeapIndex != UINT32_MAX) {
                D3D12_CPU_DESCRIPTOR_HANDLE Handle = _SrvHeap->GetCPUDescriptorHandleForHeapStart();
                Handle.ptr += CAST<UINT64>(Tex.SrvHeapIndex) * _SrvDescriptorSize;

                D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc {};
                SrvDesc.Format = WantsDepthTarget ? ToDepthSrvFormat(Desc.Fmt) : Tex.Format;
                SrvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
                SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                SrvDesc.Texture2D.MipLevels     = ResDesc.MipLevels;
                _Device->CreateShaderResourceView(Tex.Resource.Get(), &SrvDesc, Handle);
            }
        }

        if (WantsColorTarget) {
            Tex.RtvHeapIndex = AllocateOffscreenRtvSlot();
            if (Tex.RtvHeapIndex != UINT32_MAX) {
                D3D12_CPU_DESCRIPTOR_HANDLE Handle = _OffscreenRtvHeap->GetCPUDescriptorHandleForHeapStart();
                Handle.ptr += CAST<UINT64>(Tex.RtvHeapIndex) * _OffscreenRtvDescriptorSize;
                _Device->CreateRenderTargetView(Tex.Resource.Get(), nullptr, Handle);
            } else {
                LOG_ERR("offscreen RTV heap exhausted (capacity %u)", OffscreenRtvHeapCapacity);
            }
        }

        if (WantsDepthTarget) {
            Tex.DsvHeapIndex = AllocateOffscreenDsvSlot();
            if (Tex.DsvHeapIndex != UINT32_MAX) {
                D3D12_CPU_DESCRIPTOR_HANDLE Handle = _OffscreenDsvHeap->GetCPUDescriptorHandleForHeapStart();
                Handle.ptr += CAST<UINT64>(Tex.DsvHeapIndex) * _OffscreenDsvDescriptorSize;

                D3D12_DEPTH_STENCIL_VIEW_DESC DsvDesc {};
                DsvDesc.Format        = Tex.Format;
                DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                _Device->CreateDepthStencilView(Tex.Resource.Get(), &DsvDesc, Handle);
            } else {
                LOG_ERR("offscreen DSV heap exhausted (capacity %u)", OffscreenDsvHeapCapacity);
            }
        }

        return _Textures.Allocate(std::move(Tex));
    }

    void D3D12RenderDevice::UploadTexture(const TextureHandle Handle, const TextureUploadDesc& Upload) {
        D3DTexture* Tex = _Textures.Get(Handle);
        if (!Tex || !Upload.Data) return;

        D3D12_RESOURCE_DESC ResDesc = Tex->Resource->GetDesc();

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint {};
        UINT NumRows           = 0;
        UINT64 RowSizeInBytes  = 0;
        UINT64 TotalBytes      = 0;
        const UINT Subresource = Upload.MipLevel + Upload.ArrayLayer * ResDesc.MipLevels;

        _Device->GetCopyableFootprints(&ResDesc, Subresource, 1, 0, &Footprint, &NumRows, &RowSizeInBytes, &TotalBytes);

        D3D12MA::ALLOCATION_DESC UploadAllocDesc {};
        UploadAllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC UploadResDesc {};
        UploadResDesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        UploadResDesc.Width            = TotalBytes;
        UploadResDesc.Height           = 1;
        UploadResDesc.DepthOrArraySize = 1;
        UploadResDesc.MipLevels        = 1;
        UploadResDesc.SampleDesc.Count = 1;
        UploadResDesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ComPtr<D3D12MA::Allocation> UploadAlloc;
        ComPtr<ID3D12Resource> UploadRes;
        if (FAILED(_Allocator->CreateResource(&UploadAllocDesc,
                                              &UploadResDesc,
                                              D3D12_RESOURCE_STATE_GENERIC_READ,
                                              nullptr,
                                              &UploadAlloc,
                                              IID_PPV_ARGS(&UploadRes))))
            return;

        u8* Mapped = nullptr;
        UploadRes->Map(0, nullptr, RCAST<void**>(&Mapped));

        const auto* Src    = CAST<const u8*>(Upload.Data);
        const u64 SrcPitch = Upload.Width * 4;  // RGBA8 - the only format the texture cache uploads today
        for (UINT Row = 0; Row < NumRows; ++Row) {
            std::memcpy(Mapped + Footprint.Offset + CAST<u64>(Row) * Footprint.Footprint.RowPitch,
                        Src + Row * SrcPitch,
                        SrcPitch);
        }
        UploadRes->Unmap(0, nullptr);

        ExecuteUploadAndWait([&](ID3D12GraphicsCommandList* List) {
            D3D12_RESOURCE_BARRIER ToCopyDest {};
            ToCopyDest.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            ToCopyDest.Transition.pResource   = Tex->Resource.Get();
            ToCopyDest.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            ToCopyDest.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
            ToCopyDest.Transition.Subresource = Subresource;
            List->ResourceBarrier(1, &ToCopyDest);

            D3D12_TEXTURE_COPY_LOCATION Dst {};
            Dst.pResource        = Tex->Resource.Get();
            Dst.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            Dst.SubresourceIndex = Subresource;

            D3D12_TEXTURE_COPY_LOCATION SrcLoc {};
            SrcLoc.pResource       = UploadRes.Get();
            SrcLoc.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            SrcLoc.PlacedFootprint = Footprint;

            List->CopyTextureRegion(&Dst, 0, 0, 0, &SrcLoc, nullptr);

            // Combined so a compute pass (post-process reading a texture) can
            // sample it too, not just a pixel shader.
            constexpr D3D12_RESOURCE_STATES ShaderReadable =
              D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

            D3D12_RESOURCE_BARRIER ToShaderResource {};
            ToShaderResource.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            ToShaderResource.Transition.pResource   = Tex->Resource.Get();
            ToShaderResource.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            ToShaderResource.Transition.StateAfter  = ShaderReadable;
            ToShaderResource.Transition.Subresource = Subresource;
            List->ResourceBarrier(1, &ToShaderResource);
        });

        // ExecuteUploadAndWait runs and waits synchronously above, so the GPU
        // has already executed the barrier by the time we get here - safe to
        // update the tracked state on this thread with no race.
        Tex->CurrentState =
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }

    void D3D12RenderDevice::DestroyTexture(const TextureHandle Handle) {
        if (D3DTexture* Tex = _Textures.Get(Handle)) _RetiredTextures.push_back({_NextFenceValue, std::move(*Tex)});
        _Textures.Free(Handle);
    }

    // --- Resources: samplers, shaders, pipelines ---------------------------

    SamplerHandle D3D12RenderDevice::CreateSampler(const SamplerDesc& Desc) {
        D3DSampler Samp;
        Samp.HeapIndex = AllocateSamplerSlot();
        if (Samp.HeapIndex == UINT32_MAX) return {};

        D3D12_SAMPLER_DESC SamplerDesc_ {};
        SamplerDesc_.Filter   = ToD3DFilter(Desc.MinFilter, Desc.MagFilter, Desc.MipFilter, Desc.MaxAnisotropy > 1);
        SamplerDesc_.AddressU = ToD3DAddressMode(Desc.AddressU);
        SamplerDesc_.AddressV = ToD3DAddressMode(Desc.AddressV);
        SamplerDesc_.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        SamplerDesc_.MaxAnisotropy  = std::max<UINT>(Desc.MaxAnisotropy, 1);
        SamplerDesc_.MinLOD         = Desc.MinLod;
        SamplerDesc_.MaxLOD         = Desc.MaxLod;
        SamplerDesc_.MipLODBias     = Desc.MipLodBias;
        SamplerDesc_.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

        D3D12_CPU_DESCRIPTOR_HANDLE Handle = _SamplerHeap->GetCPUDescriptorHandleForHeapStart();
        Handle.ptr += CAST<UINT64>(Samp.HeapIndex) * _SamplerDescriptorSize;
        _Device->CreateSampler(&SamplerDesc_, Handle);

        return _Samplers.Allocate(std::move(Samp));
    }

    void D3D12RenderDevice::DestroySampler(const SamplerHandle Handle) {
        if (D3DSampler* Samp = _Samplers.Get(Handle)) _RetiredSamplers.push_back({_NextFenceValue, std::move(*Samp)});
        _Samplers.Free(Handle);
    }

    ShaderHandle D3D12RenderDevice::CreateShader(const ShaderDesc& Desc) {
        // Already-compiled DXIL (offline, via dxc.exe - see Scripts/
        // compile_engine_shaders.py): wrap the bytes directly, no
        // compilation step, no entry point (baked into the container).
        if (Desc.SourceType == ShaderSourceType::DXIL) {
            ComPtr<ID3DBlob> Blob;
            if (FAILED(D3DCreateBlob(Desc.CodeSize, &Blob))) {
                LOG_ERR("failed to allocate blob for precompiled shader (%s)", Desc.DebugName ? Desc.DebugName : "?");
                return {};
            }
            std::memcpy(Blob->GetBufferPointer(), Desc.Code, Desc.CodeSize);

            D3DShader Obj;
            Obj.Bytecode = Blob;
            Obj.Stage    = Desc.Stage;
            return _Shaders.Allocate(std::move(Obj));
        }

        const char* Target = nullptr;
        switch (Desc.Stage) {
            case ShaderStage::Vertex:
                Target = "vs_5_1";
                break;
            case ShaderStage::Fragment:
                Target = "ps_5_1";
                break;
            case ShaderStage::Compute:
                Target = "cs_5_1";
                break;
            default:
                return {};
        }

        UINT Flags = 0;
#ifndef NDEBUG
        Flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        ComPtr<ID3DBlob> Code, Errors;
        const HRESULT Hr = D3DCompile(Desc.Code,
                                      Desc.CodeSize,
                                      Desc.DebugName,
                                      nullptr,
                                      nullptr,
                                      Desc.EntryPoint,
                                      Target,
                                      Flags,
                                      0,
                                      &Code,
                                      &Errors);
        if (FAILED(Hr)) {
            LOG_ERR("shader compile failed (%s): %s",
                    Desc.DebugName ? Desc.DebugName : "?",
                    Errors ? CAST<const char*>(Errors->GetBufferPointer()) : "unknown error");
            return {};
        }

        D3DShader Obj;
        Obj.Bytecode = Code;
        Obj.Stage    = Desc.Stage;
        return _Shaders.Allocate(std::move(Obj));
    }

    void D3D12RenderDevice::DestroyShader(const ShaderHandle Handle) {
        if (D3DShader* Shader = _Shaders.Get(Handle)) _RetiredShaders.push_back({_NextFenceValue, std::move(*Shader)});
        _Shaders.Free(Handle);
    }

    // --- Resources: pipeline layouts ----------------------------------------

    LayoutHandle D3D12RenderDevice::CreatePipelineLayout(const PipelineLayoutDesc& Desc) {
        D3DPipelineLayout Layout;

        std::vector<D3D12_ROOT_PARAMETER> RootParams;
        RootParams.reserve(Desc.BindingCount);

        // Reserved up front and never reallocated after: root parameters
        // below take pointers into this vector, and Desc.BindingCount is an
        // exact upper bound on how many ranges get pushed (at most one per
        // binding), so those pointers stay valid through CreateRootSignature.
        std::vector<D3D12_DESCRIPTOR_RANGE> Ranges;
        Ranges.reserve(Desc.BindingCount);

        for (u8 i = 0; i < Desc.BindingCount; ++i) {
            const BindingSlot& B = Desc.Bindings[i];

            D3DPipelineLayout::ResolvedBinding Resolved;
            Resolved.Slot               = B.Slot;
            Resolved.Type               = B.Type;
            Resolved.RootParameterIndex = CAST<u32>(RootParams.size());

            // A lone uniform buffer gets a root CBV - no descriptor-heap
            // indirection for the case every draw call touches (per-frame/
            // per-object constants). Anything else (arrays, textures,
            // samplers, UAVs) goes through a one-range descriptor table.
            if (B.Type == BindingType::UniformBuffer && B.Count == 1) {
                D3D12_ROOT_PARAMETER Param {};
                Param.ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
                Param.Descriptor.ShaderRegister = B.Slot;
                Param.ShaderVisibility          = ToD3DVisibility(B.Visibility);
                RootParams.push_back(Param);
                Resolved.IsTable = false;
            } else {
                D3D12_DESCRIPTOR_RANGE Range {};
                Range.RangeType                         = ToD3DRangeType(B.Type);
                Range.NumDescriptors                    = B.Count;
                Range.BaseShaderRegister                = B.Slot;
                Range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                Ranges.push_back(Range);

                D3D12_ROOT_PARAMETER Param {};
                Param.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                Param.DescriptorTable.NumDescriptorRanges = 1;
                Param.DescriptorTable.pDescriptorRanges   = &Ranges.back();
                Param.ShaderVisibility                    = ToD3DVisibility(B.Visibility);
                RootParams.push_back(Param);
                Resolved.IsTable = true;
            }

            Layout.Bindings[Layout.BindingCount++] = Resolved;
        }

        D3D12_ROOT_SIGNATURE_DESC RootDesc {};
        RootDesc.NumParameters = CAST<UINT>(RootParams.size());
        RootDesc.pParameters   = RootParams.empty() ? nullptr : RootParams.data();
        RootDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> SigBlob, ErrBlob;
        if (FAILED(D3D12SerializeRootSignature(&RootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &SigBlob, &ErrBlob))) {
            LOG_ERR("pipeline layout root signature serialize failed (%s): %s",
                    Desc.DebugName ? Desc.DebugName : "?",
                    ErrBlob ? CAST<const char*>(ErrBlob->GetBufferPointer()) : "unknown error");
            return {};
        }
        if (FAILED(_Device->CreateRootSignature(
              0, SigBlob->GetBufferPointer(), SigBlob->GetBufferSize(), IID_PPV_ARGS(&Layout.RootSignature))))
            return {};

        return _Layouts.Allocate(std::move(Layout));
    }

    void D3D12RenderDevice::DestroyPipelineLayout(const LayoutHandle Handle) {
        if (D3DPipelineLayout* Layout = _Layouts.Get(Handle))
            _RetiredLayouts.push_back({_NextFenceValue, std::move(*Layout)});
        _Layouts.Free(Handle);
    }

    PipelineHandle D3D12RenderDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc& Desc) {
        const D3DShader* VS               = _Shaders.Get(Desc.VertexShader);
        const D3DShader* PS               = _Shaders.Get(Desc.FragmentShader);
        const D3DPipelineLayout* LayoutPtr = _Layouts.Get(Desc.PipelineLayout);
        if (!VS || !PS || !LayoutPtr) return {};

        std::vector<D3D12_INPUT_ELEMENT_DESC> InputElements;
        InputElements.reserve(Desc.Layout.AttributeCount);
        for (u8 i = 0; i < Desc.Layout.AttributeCount; ++i) {
            const VertexAttribute& Attr = Desc.Layout.Attributes[i];

            const VertexBufferBinding* Binding = nullptr;
            for (u8 b = 0; b < Desc.Layout.BindingCount; ++b) {
                if (Desc.Layout.Bindings[b].Binding == Attr.Binding) {
                    Binding = &Desc.Layout.Bindings[b];
                    break;
                }
            }
            const bool PerInstance = Binding && Binding->InputRate == VertexInputRate::Instance;

            D3D12_INPUT_ELEMENT_DESC Elem {};
            Elem.SemanticName      = "TEXCOORD";
            Elem.SemanticIndex     = Attr.Location;
            Elem.Format            = ToDXGIFormat(Attr.Fmt);
            Elem.InputSlot         = Attr.Binding;
            Elem.AlignedByteOffset = Attr.Offset;
            Elem.InputSlotClass =
              PerInstance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            Elem.InstanceDataStepRate = PerInstance ? std::max<u32>(Binding->Divisor, 1) : 0;
            InputElements.push_back(Elem);
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc {};
        PsoDesc.pRootSignature        = LayoutPtr->RootSignature.Get();
        PsoDesc.VS                    = {VS->Bytecode->GetBufferPointer(), VS->Bytecode->GetBufferSize()};
        PsoDesc.PS                    = {PS->Bytecode->GetBufferPointer(), PS->Bytecode->GetBufferSize()};
        PsoDesc.InputLayout           = {InputElements.data(), CAST<UINT>(InputElements.size())};
        PsoDesc.PrimitiveTopologyType = ToD3DTopologyType(Desc.Topology);
        PsoDesc.SampleMask            = UINT_MAX;
        PsoDesc.SampleDesc.Count      = std::max<u32>(Desc.SampleCount, 1);

        PsoDesc.RasterizerState.FillMode = ToD3DFillMode(Desc.Rasterizer.Fill);
        PsoDesc.RasterizerState.CullMode = ToD3DCullMode(Desc.Rasterizer.Cull);
        // Inverted, not a direct CCW->TRUE mapping: D3D12's front-face test
        // happens on screen-space winding, after the NDC->viewport transform
        // flips Y (NDC is Y-up, pixel/screen space is Y-down) - a triangle
        // authored CCW in the standard Y-up math convention (how every mesh
        // this engine generates is wound, e.g. Scripts/generate_primitive_
        // meshes.py) comes out CW once that flip happens, so it has to map
        // to FrontCounterClockwise=FALSE to still be treated as front.
        // Confirmed empirically: a straight (non-inverted) mapping rendered
        // a cube's far faces instead of its near ones (depth-tested visible
        // geometry using the far face's own normal).
        PsoDesc.RasterizerState.FrontCounterClockwise = Desc.Rasterizer.Front != FrontFace::CounterClockwise;
        PsoDesc.RasterizerState.DepthClipEnable       = TRUE;

        PsoDesc.DepthStencilState.DepthEnable = Desc.DepthStencil.DepthTestEnable;
        PsoDesc.DepthStencilState.DepthWriteMask =
          Desc.DepthStencil.DepthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        PsoDesc.DepthStencilState.DepthFunc        = ToD3DCompareOp(Desc.DepthStencil.DepthCompare);
        PsoDesc.DepthStencilState.StencilEnable    = Desc.DepthStencil.StencilEnable;
        PsoDesc.DepthStencilState.StencilReadMask  = Desc.DepthStencil.StencilReadMask;
        PsoDesc.DepthStencilState.StencilWriteMask = Desc.DepthStencil.StencilWriteMask;
        PsoDesc.DepthStencilState.FrontFace        = {ToD3DStencilOp(Desc.DepthStencil.Front.FailOp),
                                                      ToD3DStencilOp(Desc.DepthStencil.Front.DepthFailOp),
                                                      ToD3DStencilOp(Desc.DepthStencil.Front.PassOp),
                                                      ToD3DCompareOp(Desc.DepthStencil.Front.Compare)};
        PsoDesc.DepthStencilState.BackFace         = {ToD3DStencilOp(Desc.DepthStencil.Back.FailOp),
                                                      ToD3DStencilOp(Desc.DepthStencil.Back.DepthFailOp),
                                                      ToD3DStencilOp(Desc.DepthStencil.Back.PassOp),
                                                      ToD3DCompareOp(Desc.DepthStencil.Back.Compare)};

        PsoDesc.BlendState.IndependentBlendEnable = Desc.Blend.IndependentBlend;
        const u32 BlendTargets                    = Desc.Blend.IndependentBlend ? MAX_COLOR_ATTACHMENTS : 1;
        for (u32 i = 0; i < BlendTargets; ++i) {
            const BlendAttachmentState& Src     = Desc.Blend.Attachments[i];
            D3D12_RENDER_TARGET_BLEND_DESC& Dst = PsoDesc.BlendState.RenderTarget[Desc.Blend.IndependentBlend ? i : 0];
            Dst.BlendEnable                     = Src.BlendEnable;
            Dst.SrcBlend                        = ToD3DBlendFactor(Src.SrcColor);
            Dst.DestBlend                       = ToD3DBlendFactor(Src.DstColor);
            Dst.BlendOp                         = ToD3DBlendOp(Src.ColorOp);
            Dst.SrcBlendAlpha                   = ToD3DBlendFactor(Src.SrcAlpha);
            Dst.DestBlendAlpha                  = ToD3DBlendFactor(Src.DstAlpha);
            Dst.BlendOpAlpha                    = ToD3DBlendOp(Src.AlphaOp);
            Dst.RenderTargetWriteMask           = CAST<UINT8>(Src.WriteMask);
        }
        if (!Desc.Blend.IndependentBlend) {
            for (u32 i = 1; i < Desc.ColorAttachmentCount; ++i)
                PsoDesc.BlendState.RenderTarget[i] = PsoDesc.BlendState.RenderTarget[0];
        }

        PsoDesc.NumRenderTargets = Desc.ColorAttachmentCount;
        for (u32 i = 0; i < Desc.ColorAttachmentCount; ++i)
            PsoDesc.RTVFormats[i] = ToDXGIFormat(Desc.ColorFormats[i]);
        PsoDesc.DSVFormat = Desc.DepthFormat == Format::Unknown ? DXGI_FORMAT_UNKNOWN : ToDXGIFormat(Desc.DepthFormat);

        ComPtr<ID3D12PipelineState> PSO;
        if (FAILED(_Device->CreateGraphicsPipelineState(&PsoDesc, IID_PPV_ARGS(&PSO)))) return {};

        D3DPipeline Pipe;
        Pipe.PSO      = PSO;
        Pipe.Layout   = Desc.PipelineLayout;
        Pipe.Topology = ToD3DTopology(Desc.Topology);
        for (u8 b = 0; b < Desc.Layout.BindingCount; ++b) {
            Pipe.Strides[Desc.Layout.Bindings[b].Binding] = Desc.Layout.Bindings[b].Stride;
        }

        return _Pipelines.Allocate(std::move(Pipe));
    }

    PipelineHandle D3D12RenderDevice::CreateComputePipeline(const ComputePipelineDesc& Desc) {
        const D3DShader* CS               = _Shaders.Get(Desc.ComputeShader);
        const D3DPipelineLayout* LayoutPtr = _Layouts.Get(Desc.PipelineLayout);
        if (!CS || !LayoutPtr) return {};

        D3D12_COMPUTE_PIPELINE_STATE_DESC PsoDesc {};
        PsoDesc.pRootSignature = LayoutPtr->RootSignature.Get();
        PsoDesc.CS             = {CS->Bytecode->GetBufferPointer(), CS->Bytecode->GetBufferSize()};

        ComPtr<ID3D12PipelineState> PSO;
        if (FAILED(_Device->CreateComputePipelineState(&PsoDesc, IID_PPV_ARGS(&PSO)))) return {};

        D3DPipeline Pipe;
        Pipe.PSO       = PSO;
        Pipe.Layout    = Desc.PipelineLayout;
        Pipe.IsCompute = true;
        return _Pipelines.Allocate(std::move(Pipe));
    }

    void D3D12RenderDevice::DestroyPipeline(const PipelineHandle Handle) {
        if (D3DPipeline* Pipe = _Pipelines.Get(Handle))
            _RetiredPipelines.push_back({_NextFenceValue, std::move(*Pipe)});
        _Pipelines.Free(Handle);
    }

    // --- Transient allocation ------------------------------------------------

    TransientAllocation D3D12RenderDevice::AllocateTransient(const u32 Size, BufferUsage) {
        return _Transient.Allocate(Size, _Transient.GetHandle(_FrameIndex));
    }

    std::unique_ptr<IRenderDevice> CreateRenderDevice(const Backend API) {
        switch (API) {
            case Backend::D3D12:
                return std::make_unique<D3D12RenderDevice>();
            default:
                return nullptr;
        }
    }
}  // namespace Xen::RHI::D3D12Backend

namespace Xen::RHI {
    std::unique_ptr<IRenderDevice> CreateRenderDevice(const Backend API) {
        return D3D12Backend::CreateRenderDevice(API);
    }
}  // namespace Xen::RHI
