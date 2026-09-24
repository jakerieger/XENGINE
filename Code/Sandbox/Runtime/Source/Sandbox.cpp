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

            // Rolling history of the line above, not the CPU-side Frame
            // Stats delta - a fixed-size circular buffer (~4s at 60 FPS),
            // static so it persists across calls without adding a member to
            // Sandbox for a debug-only graph. Only fed once real timing data
            // exists (this whole block is skipped otherwise), so the graph
            // never gets polluted with the all-zero startup frames.
            static float FrameTimeHistory[240] = {};
            static int FrameTimeOffset         = 0;
            static bool FrameTimeFilled         = false;

            FrameTimeHistory[FrameTimeOffset] = TopLevelTotal;
            FrameTimeOffset                   = (FrameTimeOffset + 1) % IM_ARRAYSIZE(FrameTimeHistory);
            if (FrameTimeOffset == 0) FrameTimeFilled = true;

            const int SampleCount = FrameTimeFilled ? IM_ARRAYSIZE(FrameTimeHistory) : FrameTimeOffset;
            float MaxSample        = 0.0f;
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
                             MaxSample * 1.2f + 0.001f,  // +epsilon: a perfectly flat 0ms history would else divide by zero
                             ImVec2(0.0f, 80.0f));
        }
    }
    ImGui::End();

    // The engine's own ring-buffer logger (Common/Log.hpp) - every LOG_INFO/
    // WARN/ERR/CRIT/DBG call anywhere in the process, not just this demo's
    // own. The buffer itself is a fixed-size ring (Logger::LOGGER_MAX_ENTRIES,
    // currently 4096); snapshotted under its mutex (AssetLoader's worker
    // threads log too) into a local copy first, so the actual ImGui:: calls
    // - the slow part - run unlocked.
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