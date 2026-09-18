//
// Created by Jake Rieger on 9/18/2026.
//

#include "DebugUI.hpp"

#if XEN_WITH_DEBUG_UI

    #include <Common/Log.hpp>

    #include "Window.hpp"
    #include "Backends/D3D12RenderDevice.hpp"

    #include <imgui.h>
    #include <imgui_impl_win32.h>
    #include <imgui_impl_dx12.h>

    #include <vector>

// imgui_impl_win32.h intentionally leaves this commented out (behind
// #if 0) to keep <Windows.h> out of that header for callers who don't need
// it - this file does (via DebugUI.hpp), so it forward-declares it itself,
// exactly as that header's own comment instructs.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Xen {
    struct DebugUI::Impl {
        RHI::D3D12Backend::D3D12RenderDevice* Device {nullptr};
        ImGuiContext* Context {nullptr};

        // Entirely separate from the render device's own SRV heap: Dear
        // ImGui needs a shader-visible heap it fully controls (the docking
        // branch's dynamic font-atlas support means it can allocate more
        // descriptors than just the font atlas over the app's lifetime, via
        // the Alloc/Free callbacks below), and reusing the engine's heap
        // would mean reaching into D3D12RenderDevice's private SRV slot
        // bookkeeping for a debug-only feature.
        static constexpr u32 SrvHeapCapacity = 64;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> SrvHeap;
        u32 SrvDescriptorSize {0};
        std::vector<bool> SrvSlotUsed;

        static void SrvAlloc(ImGui_ImplDX12_InitInfo* Info,
                             D3D12_CPU_DESCRIPTOR_HANDLE* OutCpu,
                             D3D12_GPU_DESCRIPTOR_HANDLE* OutGpu) {
            auto* Self = CAST<Impl*>(Info->UserData);
            for (u32 i = 0; i < SrvHeapCapacity; ++i) {
                if (Self->SrvSlotUsed[i]) continue;
                Self->SrvSlotUsed[i] = true;

                D3D12_CPU_DESCRIPTOR_HANDLE Cpu = Self->SrvHeap->GetCPUDescriptorHandleForHeapStart();
                Cpu.ptr += CAST<SIZE_T>(i) * Self->SrvDescriptorSize;
                D3D12_GPU_DESCRIPTOR_HANDLE Gpu = Self->SrvHeap->GetGPUDescriptorHandleForHeapStart();
                Gpu.ptr += CAST<UINT64>(i) * Self->SrvDescriptorSize;

                *OutCpu = Cpu;
                *OutGpu = Gpu;
                return;
            }

            LOG_ERR("DebugUI: out of SRV descriptor slots (capacity=%u) - a texture won't display", SrvHeapCapacity);
            *OutCpu = {};
            *OutGpu = {};
        }

        static void
        SrvFree(ImGui_ImplDX12_InitInfo* Info, const D3D12_CPU_DESCRIPTOR_HANDLE Cpu, D3D12_GPU_DESCRIPTOR_HANDLE) {
            auto* Self                                 = CAST<Impl*>(Info->UserData);
            const D3D12_CPU_DESCRIPTOR_HANDLE HeapStart = Self->SrvHeap->GetCPUDescriptorHandleForHeapStart();
            const auto Index = CAST<u32>((Cpu.ptr - HeapStart.ptr) / Self->SrvDescriptorSize);
            if (Index < SrvHeapCapacity) Self->SrvSlotUsed[Index] = false;
        }
    };

    DebugUI::DebugUI()  = default;
    DebugUI::~DebugUI() {
        Shutdown();
    }

    bool DebugUI::Initialize(RHI::IRenderDevice& Device, Window& AppWindow) {
        if (_Initialized) return true;

        if (Device.GetBackend() != RHI::Backend::D3D12) {
            LOG_ERR("DebugUI requires the D3D12 backend");
            return false;
        }

        _Impl         = std::make_unique<Impl>();
        _Impl->Device = static_cast<RHI::D3D12Backend::D3D12RenderDevice*>(&Device);

        IMGUI_CHECKVERSION();
        _Impl->Context = ImGui::CreateContext();
        ImGui::SetCurrentContext(_Impl->Context);

        ImGuiIO& IO = ImGui::GetIO();
        IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        // Debug window layout persists across launches like any other engine
        // config, rather than littering the working directory with a stray
        // imgui.ini next to the exe.
        IO.IniFilename = "Config/DebugUI.ini";

        ImGui::StyleColorsDark();

        if (!ImGui_ImplWin32_Init(AppWindow.GetHandle())) {
            LOG_ERR("DebugUI: ImGui_ImplWin32_Init failed");
            Shutdown();
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc {};
        HeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        HeapDesc.NumDescriptors = Impl::SrvHeapCapacity;
        HeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(_Impl->Device->GetD3DDevice()->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&_Impl->SrvHeap)))) {
            LOG_ERR("DebugUI: failed to create SRV descriptor heap");
            Shutdown();
            return false;
        }
        _Impl->SrvDescriptorSize =
          _Impl->Device->GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        _Impl->SrvSlotUsed.assign(Impl::SrvHeapCapacity, false);

        ImGui_ImplDX12_InitInfo InitInfo {};
        InitInfo.Device               = _Impl->Device->GetD3DDevice();
        InitInfo.CommandQueue         = _Impl->Device->GetD3DCommandQueue();
        InitInfo.NumFramesInFlight    = CAST<int>(_Impl->Device->GetFramesInFlight());
        InitInfo.RTVFormat            = _Impl->Device->GetSwapChainFormat();
        InitInfo.DSVFormat            = DXGI_FORMAT_UNKNOWN;
        InitInfo.SrvDescriptorHeap    = _Impl->SrvHeap.Get();
        InitInfo.UserData             = _Impl.get();
        InitInfo.SrvDescriptorAllocFn = &Impl::SrvAlloc;
        InitInfo.SrvDescriptorFreeFn  = &Impl::SrvFree;

        if (!ImGui_ImplDX12_Init(&InitInfo)) {
            LOG_ERR("DebugUI: ImGui_ImplDX12_Init failed");
            Shutdown();
            return false;
        }

        _Initialized = true;
        LOG_DBG("DebugUI initialized (Dear ImGui %s)", IMGUI_VERSION);
        return true;
    }

    void DebugUI::Shutdown() {
        if (!_Impl) return;

        if (_Impl->Context) {
            // The GPU may still be reading Dear ImGui's font texture/SRV
            // heap from an in-flight frame's command list - unlike the
            // engine's own resources (see D3D12RenderDevice's deferred-
            // delete queue), these are raw COM objects with no fenced
            // retirement of their own, so they're only safe to release once
            // the device is known idle.
            _Impl->Device->WaitIdle();

            ImGui::SetCurrentContext(_Impl->Context);
            ImGui_ImplDX12_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext(_Impl->Context);
        }

        _Impl.reset();
        _Initialized = false;
    }

    void DebugUI::BeginFrame() {
        if (!_Initialized) return;

        ImGui::SetCurrentContext(_Impl->Context);
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void DebugUI::EndFrame() {
        if (!_Initialized) return;

        ImGui::SetCurrentContext(_Impl->Context);
        ImGui::Render();

        _Impl->Device->BindSwapChainOverlayTarget();

        ID3D12GraphicsCommandList* CmdList = _Impl->Device->GetD3DCommandList();
        ID3D12DescriptorHeap* Heaps[]      = {_Impl->SrvHeap.Get()};
        CmdList->SetDescriptorHeaps(1, Heaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), CmdList);

        _Impl->Device->UnbindSwapChainOverlayTarget();
    }

    bool DebugUI::ProcessMessage(const HWND Handle, const UINT Msg, const WPARAM WParam, const LPARAM LParam) {
        if (!_Initialized) return false;
        ImGui::SetCurrentContext(_Impl->Context);
        return ImGui_ImplWin32_WndProcHandler(Handle, Msg, WParam, LParam) != 0;
    }

    bool DebugUI::WantsCaptureMouse() const {
        if (!_Initialized) return false;
        ImGui::SetCurrentContext(_Impl->Context);
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool DebugUI::WantsCaptureKeyboard() const {
        if (!_Initialized) return false;
        ImGui::SetCurrentContext(_Impl->Context);
        return ImGui::GetIO().WantCaptureKeyboard;
    }
}  // namespace Xen

#else

namespace Xen {
    struct DebugUI::Impl {};

    DebugUI::DebugUI()  = default;
    DebugUI::~DebugUI() = default;

    bool DebugUI::Initialize(RHI::IRenderDevice&, Window&) {
        return false;
    }
    void DebugUI::Shutdown() {}
    void DebugUI::BeginFrame() {}
    void DebugUI::EndFrame() {}
    bool DebugUI::ProcessMessage(HWND, UINT, WPARAM, LPARAM) {
        return false;
    }
    bool DebugUI::WantsCaptureMouse() const {
        return false;
    }
    bool DebugUI::WantsCaptureKeyboard() const {
        return false;
    }
}  // namespace Xen

#endif
