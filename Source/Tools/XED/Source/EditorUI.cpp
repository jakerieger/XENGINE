//
// Created by Jake Rieger on 9/28/2026.
//

#include "EditorUI.hpp"

#include <Common/Log.hpp>
#include <Xen/Window.hpp>
#include <Xen/Backends/D3D12RenderDevice.hpp>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <map>
#include <vector>

// imgui_impl_win32.h intentionally leaves this commented out (behind
// #if 0) to keep <Windows.h> out of that header for callers who don't need
// it - this file does (via EditorUI.hpp), so it forward-declares it itself,
// exactly as that header's own comment instructs.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Xen {
    struct EditorUI::Impl {
        RHI::D3D12Backend::D3D12RenderDevice* Device {nullptr};
        ImGuiContext* Context {nullptr};
        std::map<std::string, ImFont*> Fonts;

        // Entirely separate from the render device's own SRV heap: Dear
        // ImGui needs a shader-visible heap it fully controls (the docking
        // branch's dynamic font-atlas support means it can allocate more
        // descriptors than just the font atlas over the app's lifetime, via
        // the Alloc/Free callbacks below), and reusing the engine's heap
        // would mean reaching into D3D12RenderDevice's private SRV slot
        // bookkeeping for an editor-only feature.
        static constexpr u32 SrvHeapCapacity = 64;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> SrvHeap;
        u32 SrvDescriptorSize {0};
        std::vector<bool> SrvSlotUsed;

        // Lazily allocated by GetOrCreateSceneTextureID, from this same
        // heap/free-list - one fixed slot, rewritten every call rather than
        // one slot per distinct TextureHandle (see that method's own
        // comment for why).
        u32 SceneTextureSlot {UINT32_MAX};

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

            LOG_ERR("EditorUI: out of SRV descriptor slots (capacity=%u) - a texture won't display", SrvHeapCapacity);
            *OutCpu = {};
            *OutGpu = {};
        }

        static void
        SrvFree(ImGui_ImplDX12_InitInfo* Info, const D3D12_CPU_DESCRIPTOR_HANDLE Cpu, D3D12_GPU_DESCRIPTOR_HANDLE) {
            auto* Self                                  = CAST<Impl*>(Info->UserData);
            const D3D12_CPU_DESCRIPTOR_HANDLE HeapStart = Self->SrvHeap->GetCPUDescriptorHandleForHeapStart();
            const auto Index = CAST<u32>((Cpu.ptr - HeapStart.ptr) / Self->SrvDescriptorSize);
            if (Index < SrvHeapCapacity) Self->SrvSlotUsed[Index] = false;
        }
    };

    EditorUI::EditorUI() = default;
    EditorUI::~EditorUI() {
        Shutdown();
    }

    bool EditorUI::Initialize(RHI::IRenderDevice& Device, const Window& AppWindow) {
        if (_Initialized) return true;

        if (Device.GetBackend() != RHI::Backend::D3D12) {
            LOG_ERR("EditorUI requires the D3D12 backend");
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
        // Editor layout persists across launches like any other engine
        // config, rather than littering the working directory with a stray
        // imgui.ini next to the exe. Deliberately a different filename from
        // DebugUI's own Config/DebugUI.ini - these are two unrelated
        // ImGuiContexts with two unrelated layouts.
        IO.IniFilename = "Config/EditorUI.ini";

        ImGui::StyleColorsDark();

        if (!ImGui_ImplWin32_Init(AppWindow.GetHandle())) {
            LOG_ERR("EditorUI: ImGui_ImplWin32_Init failed");
            Shutdown();
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc {};
        HeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        HeapDesc.NumDescriptors = Impl::SrvHeapCapacity;
        HeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(_Impl->Device->GetD3DDevice()->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&_Impl->SrvHeap)))) {
            LOG_ERR("EditorUI: failed to create SRV descriptor heap");
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
            LOG_ERR("EditorUI: ImGui_ImplDX12_Init failed");
            Shutdown();
            return false;
        }

        _Initialized = true;
        LOG_DBG("EditorUI initialized (Dear ImGui %s (%d))", IMGUI_VERSION, IMGUI_VERSION_NUM);
        return true;
    }

    void EditorUI::Shutdown() {
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

    void EditorUI::BeginFrame() {
        if (!_Initialized) return;

        ImGui::SetCurrentContext(_Impl->Context);
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void EditorUI::EndFrame() {
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

    bool EditorUI::ProcessMessage(const HWND Handle, const UINT Msg, const WPARAM WParam, const LPARAM LParam) {
        if (!_Initialized) return false;
        ImGui::SetCurrentContext(_Impl->Context);
        return ImGui_ImplWin32_WndProcHandler(Handle, Msg, WParam, LParam) != 0;
    }

    bool EditorUI::WantsCaptureMouse() const {
        if (!_Initialized) return false;
        ImGui::SetCurrentContext(_Impl->Context);
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool EditorUI::WantsCaptureKeyboard() const {
        if (!_Initialized) return false;
        ImGui::SetCurrentContext(_Impl->Context);
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    ImTextureID EditorUI::GetOrCreateSceneTextureID(const RHI::TextureHandle Handle) {
        if (!_Initialized) return 0;

        if (_Impl->SceneTextureSlot == UINT32_MAX) {
            for (u32 i = 0; i < Impl::SrvHeapCapacity; ++i) {
                if (_Impl->SrvSlotUsed[i]) continue;
                _Impl->SrvSlotUsed[i]   = true;
                _Impl->SceneTextureSlot = i;
                break;
            }
            if (_Impl->SceneTextureSlot == UINT32_MAX) {
                LOG_ERR("EditorUI: out of SRV descriptor slots (capacity=%u) - the scene view won't display",
                        Impl::SrvHeapCapacity);
                return 0;
            }
        }

        D3D12_CPU_DESCRIPTOR_HANDLE Cpu = _Impl->SrvHeap->GetCPUDescriptorHandleForHeapStart();
        Cpu.ptr += CAST<SIZE_T>(_Impl->SceneTextureSlot) * _Impl->SrvDescriptorSize;

        if (!_Impl->Device->CreateTextureSRV(Handle, Cpu)) return 0;

        D3D12_GPU_DESCRIPTOR_HANDLE Gpu = _Impl->SrvHeap->GetGPUDescriptorHandleForHeapStart();
        Gpu.ptr += CAST<UINT64>(_Impl->SceneTextureSlot) * _Impl->SrvDescriptorSize;
        return CAST<ImTextureID>(Gpu.ptr);
    }

    bool EditorUI::LoadFont(const std::string& Name,
                            const unsigned char* Data,
                            const size_t DataSize,
                            const f32 Pixels) const {
        // AddFontFromMemoryTTF takes ownership of Data by default and IM_FREEs
        // it when the atlas is torn down - fine for a buffer it allocated
        // itself, but Data here is caller-owned (typically a static embedded
        // resource, never heap-allocated by ImGui's own allocator), so
        // freeing it on shutdown is undefined behavior. FontDataOwnedByAtlas
        // = false keeps ownership with the caller instead; the data only
        // needs to outlive this EditorUI, which a static resource trivially
        // does.
        ImFontConfig Config;
        Config.FontDataOwnedByAtlas = false;
        _Impl->Fonts[Name] =
          ImGui::GetIO().Fonts->AddFontFromMemoryTTF((void*)Data, CAST<int>(DataSize), Pixels, &Config);
        if (_Impl->Fonts[Name] == nullptr) return false;
        return true;
    }
}  // namespace Xen
