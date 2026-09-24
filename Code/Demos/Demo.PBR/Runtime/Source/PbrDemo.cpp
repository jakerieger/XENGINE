//
// Created by Jake Rieger on 9/23/2026.
//

#include "PbrDemo.hpp"

#ifdef XEN_WITH_DEBUG_UI
    #include <imgui.h>
#endif

using namespace Xen;

void PBRDemo::OnUpdate(f32 DeltaTime) {
    if (GetInputManager().GetKeyDown(Input::KeyCode::Escape)) { Quit(); }
}

void PBRDemo::OnRender() {
    if (!GetDebugUI().IsInitialized()) return;

#ifdef XEN_WITH_DEBUG_UI
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Frame Stats")) {
        const RHI::FrameStats& Stats = GetRenderDevice().GetLastFrameStats();
        const f32 Delta              = GetLastFrameDelta();

        ImGui::Text("Frame:  %llu", GetFrameCount());
        ImGui::Text("Delta:  %.2f ms (%.0f FPS)", Delta * 1000.0f, Delta > 0.0f ? 1.0f / Delta : 0.0f);
        ImGui::Separator();
        ImGui::Text("Draw calls:      %u", Stats.DrawCalls);
        ImGui::Text("Triangles:       %u", Stats.TriangleCount);
        ImGui::Text("Render passes:   %u", Stats.RenderPasses);
        ImGui::Text("Pipeline binds:  %u (%u redundant skipped)", Stats.PipelineBinds, Stats.RedundantBindsSkipped);
        ImGui::Text("Transient bytes: %u", Stats.TransientBytesUsed);
        ImGui::Text("Meshes:          %u visible, %u culled",
                    GetMeshRenderer().GetLastVisibleMeshCount(),
                    GetMeshRenderer().GetLastCulledMeshCount());

        ImGui::Separator();
        ImGui::Text("Asset Caches");
        const TextureCache& Textures = GetTextures();
        const MeshCache& Meshes      = GetMeshes();
        ImGui::Text("  Textures: %llu (%0.2f MB)",
                    CAST<u64>(Textures.GetResidentCount()),
                    ToMB(Textures.GetResidentBytes()));
        ImGui::Text("  Meshes:   %llu (%0.2f MB)",
                    CAST<u64>(Meshes.GetResidentCount()),
                    ToMB(Meshes.GetResidentBytes()));

        ImGui::Separator();
        ImGui::Text("Memory");
        // clang-format off
                const auto& [GpuAllocatedBytes,
                             GpuReservedBytes,
                             GpuUsageBytes,
                             GpuBudgetBytes,
                             ProcessRamBytes] = GetRenderDevice().GetMemoryStats();
        // clang-format on
        ImGui::Text("  GPU allocated: %0.2f MB", ToMB(GpuAllocatedBytes));
        ImGui::Text("  GPU reserved:  %0.2f MB", ToMB(GpuReservedBytes));
        ImGui::Text("  GPU usage:     %0.2f / %0.2f MB", ToMB(GpuUsageBytes), ToMB(GpuBudgetBytes));
        ImGui::Text("  Process RAM:   %0.2f MB", ToMB(ProcessRamBytes));
    }
    ImGui::End();

    // Per-pass GPU time, from CommandBuffer::PushDebugGroup/
    // PopDebugGroup scopes measured with GPU timestamp queries (see
    // D3D12RenderDevice::GetLastFrameGpuTimings) - a few frames
    // behind whatever's on screen right now (a timestamp can't be
    // read back until the GPU has actually reached it, which this
    // backend only guarantees once the same swap-chain buffer comes
    // back around), not last-frame-exact the way the CPU-side Frame
    // Stats above are. Indentation follows each scope's own nesting
    // (e.g. "Depth prepass"/"SSAO" nest under "Meshes"; "Bloom",
    // "Auto exposure metering" and "Post-process composite" are its
    // siblings, all under PostProcess::Render; "FXAA" is a top-level
    // scope of its own, recorded after MeshRenderer::Render returns).
    // AlwaysAutoResize, not just an initial auto-fit: this window's
    // first few frames have no data at all (see the comment above -
    // GPU timing takes a few frames to start resolving), so sizing
    // only once on first appearance would lock in a near-empty
    // window's height and never grow to fit the real content later.
    ImGui::SetNextWindowPos(ImVec2(10, 340), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("GPU Profiler", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const std::vector<RHI::GpuScopeTiming>& Timings = GetRenderDevice().GetLastFrameGpuTimings();
        if (Timings.empty()) {
            ImGui::TextDisabled("No GPU timing data yet.");
        } else {
            f32 TopLevelTotal = 0.0f;
            for (const RHI::GpuScopeTiming& Scope : Timings) {
                if (Scope.Depth == 0) TopLevelTotal += Scope.Milliseconds;
                ImGui::Text("%*s%-24s %6.3f ms", CAST<int>(Scope.Depth) * 2, "", Scope.Name, Scope.Milliseconds);
            }
            ImGui::Separator();
            ImGui::Text("Total (top-level scopes): %.3f ms", TopLevelTotal);
        }
    }
    ImGui::End();
#endif
}

void PBRDemo::OnSceneLoaded(Scene& S) {
    LOG_INFO("Loaded scene: %s", S.GetName().c_str());
    LOG_INFO("Actors in scene: %llu", S.GetActorCount());
    LOG_INFO("Actors:");

    S.ForEachActor(
      [](const Actor& A) { LOG_INFO("  - %s (%llu component(s))", A.GetName().c_str(), A.GetComponentCount()); });
}

void PBRDemo::OnSceneUnloading(Scene& S) {
    LOG_INFO("Scene unloading: %s", S.GetName().c_str());
}