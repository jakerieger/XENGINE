//
// Created by Jake Rieger on 9/23/2026.
//

#include "Sandbox.hpp"

#ifdef XEN_WITH_DEBUG_UI
    #include <imgui.h>
    #include <Common/Log.hpp>
    #include <cstdio>
#endif

using namespace Xen;

void Sandbox::OnUpdate(f32 DeltaTime) {
    if (GetInputManager().GetKeyDown(Input::KeyCode::Escape)) { Quit(); }

    // Forces a shader recompile+pipeline reload right now, regardless of
    // whether ShaderHotReload's own file-watching noticed a change - a
    // no-op in Release (see Game::ForceReloadShaders).
    if (GetInputManager().GetKeyDown(Input::KeyCode::F9)) { ForceReloadShaders(); }
}

void Sandbox::OnRender() {
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

            static float FrameTimeHistory[240] = {};
            static int FrameTimeOffset         = 0;
            static bool FrameTimeFilled        = false;

            FrameTimeHistory[FrameTimeOffset] = TopLevelTotal;
            FrameTimeOffset                   = (FrameTimeOffset + 1) % IM_ARRAYSIZE(FrameTimeHistory);
            if (FrameTimeOffset == 0) FrameTimeFilled = true;

            const int SampleCount = FrameTimeFilled ? IM_ARRAYSIZE(FrameTimeHistory) : FrameTimeOffset;
            float MaxSample       = 0.0f;
            for (int i = 0; i < SampleCount; ++i) {
                if (FrameTimeHistory[i] > MaxSample) MaxSample = FrameTimeHistory[i];
            }

            char Overlay[32];
            std::snprintf(Overlay, sizeof(Overlay), "%.3f ms", TopLevelTotal);
            ImGui::PlotLines("##FrameTimeHistory",
                             FrameTimeHistory,
                             SampleCount,
                             FrameTimeFilled ? FrameTimeOffset : 0,
                             Overlay,
                             0.0f,
                             MaxSample * 1.2f +
                               0.001f,  // +epsilon: a perfectly flat 0ms history would else divide by zero
                             ImVec2(0.0f, 80.0f));
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(360, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Log")) {
        Logger& Log = GetLogger();

        std::vector<Logger::Entry> Snapshot;
        {
            std::lock_guard Lock(Log.GetBufferMutex());
            const auto& Entries = Log.GetEntries();
            const size_t Total  = Log.GetTotalEntries();
            // Oldest entry is at index 0 until the ring has wrapped at least
            // once (Total == LOGGER_MAX_ENTRIES), at which point the NEXT
            // write slot (CurrentEntry) is also the oldest surviving one.
            const size_t Start = Total < Logger::LOGGER_MAX_ENTRIES ? 0 : Log.GetCurrentEntryIndex();
            Snapshot.reserve(Total);
            for (size_t i = 0; i < Total; ++i) {
                Snapshot.push_back(Entries[(Start + i) % Logger::LOGGER_MAX_ENTRIES]);
            }
        }

        if (ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
            // Only stick to the bottom on new lines if the user was already
            // there - scrolling up to read history shouldn't get yanked back
            // down by the next log line.
            const bool WasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;

            for (const Logger::Entry& Entry : Snapshot) {
                ImVec4 Color;
                switch (Entry.Severity) {
                    case Logger::Severity::Warning:
                        Color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                        break;
                    case Logger::Severity::Error:
                    case Logger::Severity::Critical:
                        Color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
                        break;
                    case Logger::Severity::Debug:
                        Color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
                        break;
                    default:
                        Color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                        break;
                }
                ImGui::TextColored(Color, "[%s] %s", Entry.TimeStamp.c_str(), Entry.Message.c_str());
            }

            if (WasAtBottom) ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }
    ImGui::End();
#endif
}

void Sandbox::OnSceneLoaded(Scene& S) {
    LOG_INFO("Loaded scene: %s", S.GetName().c_str());
    LOG_INFO("Actors in scene: %llu", S.GetActorCount());
    LOG_INFO("Actors:");

    S.ForEachActor(
      [](const Actor& A) { LOG_INFO("  - %s (%llu component(s))", A.GetName().c_str(), A.GetComponentCount()); });
}

void Sandbox::OnSceneUnloading(Scene& S) {
    LOG_INFO("Scene unloading: %s", S.GetName().c_str());
}