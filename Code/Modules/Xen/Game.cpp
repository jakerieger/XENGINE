//
// Created by Jake Rieger on 9/8/2026.
//

#include <Common/Log.hpp>

#include "Game.hpp"
#include "AssetPreloader.hpp"
#include "SceneSerializer.hpp"
#include "AssetSettings.hpp"

namespace Xen {
    PAK::AssetMountConfig BuildMountConfig(const AssetSettings& Settings, const int argc, char* argv[]) {
        PAK::AssetMountConfig Config;

        if (Settings.MountContentDirs) {
            // Dev/debug: loose content dirs cover asset loading on their own, so a
            // pak that hasn't been built yet (or hasn't been rebuilt since the last
            // content change) shouldn't be fatal - MountAssets throws on any listed
            // pak file that's missing, with no allowance for "it's optional here".
            for (const auto& Pak : Settings.PakFiles) {
                if (exists(Pak)) Config.PakFiles.push_back(Pak);
            }

            Config.ContentDirs = Settings.ContentDirs;
            if (Settings.AllowCommandLineContentDirs) { PAK::AppendContentDirsFromArgs(Config, argc, argv); }
        } else {
            // Shippable build: the pak is the only source, so a missing one is a
            // real packaging error and should still fail loudly.
            Config.PakFiles = Settings.PakFiles;
        }

        return Config;
    }

    Game::Game(const std::string& Title, const PAK::AssetMountConfig& MountConfig) {
        // Config failure is recoverable - the defaults are usable. Everything
        // after it is not, so those exceptions propagate out of the
        // constructor rather than leaving a half-built Game that null-derefs
        // on the first frame.
        try {
            _EngineConfig = EngineConfig::Read("Config/EngineConfig.ini");
            _AudioConfig  = AudioConfig::Read("Config/AudioConfig.ini");
        } catch (const std::exception& Ex) { LOG_WARN("falling back to default config: %s", Ex.what()); }

        _Window =
          std::make_unique<Window>(Title, _EngineConfig.Mode, _EngineConfig.ResolutionX, _EngineConfig.ResolutionY);

        _RenderDevice = RHI::CreateRenderDevice(RHI::Backend::D3D12);
        if (!_RenderDevice) { THROW_ENGINE_EXCEPTION(EngineException, "no render device for the requested backend"); }

        RHI::DeviceDescriptor Descriptor {};
        Descriptor.NativeWindowHandle = _Window->GetHandle();
#ifndef NDEBUG
        Descriptor.EnableValidation   = true;
        Descriptor.EnableDebugMarkers = true;
#endif

        if (!_RenderDevice->Initialize(Descriptor)) {
            THROW_ENGINE_EXCEPTION(EngineException, "failed to initialize render device");
        }

        // Not fatal if this fails (or is compiled out - see DebugUI.hpp's
        // XEN_WITH_DEBUG_UI): every call on an uninitialized DebugUI is a
        // safe no-op, so a game just doesn't get debug windows.
        if (_DebugUI.Initialize(*_RenderDevice, *_Window)) { _Window->SetDebugUI(&_DebugUI); }

        // WithDepth is always on: the cost (one extra texture) is trivial
        // next to the alternative of threading a "does this game want 3D"
        // flag through the constructor - a virtual hook wouldn't work here
        // anyway, since a subclass override isn't reachable from the base
        // constructor that runs before the derived vtable is live.
        if (!_MainViewport.Initialize(
              *_RenderDevice, _Window->GetWidth(), _Window->GetHeight(), RHI::Format::BGRA8_UNORM, true)) {
            THROW_ENGINE_EXCEPTION(EngineException, "failed to initialize main viewport");
        }

        // Without this the device's swap chain size stays 0x0, BeginRenderPass
        // sets a 0x0 viewport, and every triangle is clipped away while the
        // clear still works - which looks exactly like a correct frame with no
        // content in it.
        SetViewport(_Window->GetWidth(), _Window->GetHeight());

        if (!_SpriteRenderer.Initialize(*_RenderDevice)) {
            THROW_ENGINE_EXCEPTION(EngineException, "failed to initialize sprite renderer");
        }

        _Assets = PAK::MountAssets(MountConfig);
        if (!_Assets) { THROW_ENGINE_EXCEPTION(EngineException, "failed to mount assets"); }
        LOG_DBG("Asset mount configuration:\n%s", PAK::DescribeMounts(*_Assets).c_str());

        _Textures = std::make_unique<TextureCache>(*_Assets, *_RenderDevice);
        _Meshes   = std::make_unique<MeshCache>(*_Assets, *_RenderDevice);

        _Context.Assets   = _Assets.get();
        _Context.Owner    = this;
        _Context.Textures = _Textures.get();
        _Context.Meshes   = _Meshes.get();

        // Not fatal if this fails: a 2D-only game's content has no PBR
        // shader asset, and that's a normal, expected absence, not an error.
        if (!_MeshRenderer.Initialize(*_RenderDevice, *_Assets, _MainViewport)) {
            LOG_DBG("mesh renderer not initialized (no PBR shader asset found) - 3D rendering unavailable");
        }
    }

    Game::~Game() {
        if (_ActiveScene) TearDownActiveScene();

        // Explicit, so the renderer and viewport release their GPU resources
        // while the device is unambiguously alive. The declaration order in
        // Game.hpp would get this right anyway; doing it here makes the
        // dependency visible instead of implicit.
        _Window->SetDebugUI(nullptr);
        _DebugUI.Shutdown();
        _SpriteRenderer.Shutdown();
        _MeshRenderer.Shutdown();
        _MainViewport.Shutdown();
        _Meshes.reset();
        _Textures.reset();
    }

    void Game::Run() {
        _Running = true;

        // Load startup scene
        if (!_EngineConfig.StartupScene.empty()) {
            const AssetID SceneAsset = ASSET(_EngineConfig.StartupScene.c_str());
            if (!SceneAsset.IsValid()) {
                THROW_ENGINE_EXCEPTION(EngineException,
                                       "invalid scene asset set as startup scene: " + _EngineConfig.StartupScene);
            }

            LoadScene(SceneAsset);
        }

        OnStartup();
        ApplyPendingSceneChange();

        try {
            RunLoop();
        } catch (...) {
            TearDownActiveScene();
            _Running = false;
            OnShutdown();
            throw;
        }

        TearDownActiveScene();
        _Running = false;
        OnShutdown();
    }

    void Game::RunFrames(const u32 FrameCount) {
        _Running = true;
        OnStartup();
        ApplyPendingSceneChange();

        for (u32 i = 0; i < FrameCount && _Running; ++i) {
            TickFrame(_FixedTimeStep);
        }

        TearDownActiveScene();
        _Running = false;
        OnShutdown();
    }

    void Game::Quit() {
        _Running = false;
    }

    void Game::LoadScene(const AssetID SceneAsset) {
        _PendingSceneChange = true;
        _PendingKind        = PendingKind::LoadAsset;
        _PendingAsset       = SceneAsset;
        _PendingPath.clear();
    }

    void Game::LoadSceneFromFile(std::filesystem::path Path) {
        _PendingSceneChange = true;
        _PendingKind        = PendingKind::LoadFile;
        _PendingPath        = std::move(Path);
        _PendingAsset       = {};
    }

    void Game::UnloadScene() {
        _PendingSceneChange = true;
        _PendingKind        = PendingKind::Unload;
    }

    void Game::SetViewport(const u32 Width, const u32 Height) {
        if (_RenderDevice) _RenderDevice->SetSwapChainSize(Width, Height);
        _MainViewport.Resize(Width, Height);
        _SpriteBatcher.SetViewport(Width, Height);
    }

    void Game::RunLoop() {
        using Clock   = std::chrono::steady_clock;
        auto Previous = Clock::now();

        while (_Running && !_Window->ShouldClose()) {
            const auto Now = Clock::now();
            f32 Delta      = std::chrono::duration<f32>(Now - Previous).count();
            Previous       = Now;

            if (Delta > _MaxFrameDelta) Delta = _MaxFrameDelta;

            TickFrame(Delta);

            _Window->PollEvents();
        }
    }

    void Game::TickFrame(const f32 DeltaTime) {
        _LastDelta = DeltaTime;
        ++_FrameCount;

        _Accumulator += DeltaTime;

        u32 Steps = 0;
        while (_Accumulator >= _FixedTimeStep && Steps < _MaxFixedStepsPerFrame) {
            OnFixedUpdate(_FixedTimeStep);
            if (_ActiveScene) _ActiveScene->FixedTick(_FixedTimeStep);
            _Accumulator -= _FixedTimeStep;
            ++Steps;
        }

        if (Steps >= _MaxFixedStepsPerFrame) _Accumulator = 0.0f;

        OnUpdate(DeltaTime);

        if (_ActiveScene) _ActiveScene->Tick(DeltaTime);

        // The other half of the viewport wiring. Window::ConsumeResized is
        // edge-triggered, so this has to run every frame or a resize is lost.
        if (_Window->ConsumeResized()) { SetViewport(_Window->GetWidth(), _Window->GetHeight()); }

        // Nothing to present while minimized, and a zero-height viewport makes
        // CameraComponent::GetVisibleWorldSize divide by zero.
        if (_Window->IsMinimized()) {
            ApplyPendingSceneChange();
            return;
        }

        if (_ActiveScene) {
            // BeginFrame first: it rotates the transient ring, so any
            // AllocateTransient call made from OnRender lands in this frame's
            // arena rather than the one the GPU may still be reading.
            _RenderDevice->BeginFrame();
            _DebugUI.BeginFrame();

            _SpriteBatcher.BuildDrawList(*_ActiveScene);
            OnRender();
            _SpriteRenderer.Render(_SpriteBatcher, *_Textures, _MainViewport);

            // Runs after sprites, on top of them, depth-tested amongst
            // itself - a no-op if this game has no PBR shader asset (see
            // the constructor). See MeshRenderer::Render for why its render
            // pass loads rather than clears color: it depends on the sprite
            // pass above having already cleared this frame.
            if (_MeshRenderer.IsInitialized()) _MeshRenderer.Render(*_ActiveScene, _MainViewport);

            // Standalone-game presentation: copy the viewport's color target
            // into the back buffer. An editor wouldn't call this at all - it
            // would sample _MainViewport.GetColorTarget() into an ImGui
            // panel instead.
            _RenderDevice->CopyToSwapChain(_MainViewport.GetColorTarget());

            // Overlay, drawn after everything else so debug windows are
            // always on top - see DebugUI::EndFrame for the back-buffer
            // hand-off with CopyToSwapChain above.
            _DebugUI.EndFrame();

            _RenderDevice->EndFrame();
        }

        _Window->ResetInput();

        ApplyPendingSceneChange();
    }

    void Game::ApplyPendingSceneChange() {
        if (!_PendingSceneChange) return;

        _PendingSceneChange    = false;
        const PendingKind Kind = _PendingKind;
        const AssetID Asset    = _PendingAsset;
        const auto Path        = _PendingPath;
        _PendingKind           = PendingKind::None;

        TearDownActiveScene();

        switch (Kind) {
            case PendingKind::None:
            case PendingKind::Unload:
                return;

            case PendingKind::LoadAsset: {
                if (!_Assets->Contains(Asset)) {
                    THROW_ENGINE_EXCEPTION(EngineException, std::format("scene asset {} not found", Asset.Value));
                }

                const PAK::AssetBuffer Bytes = _Assets->Load(Asset);
                const std::string Text(RCAST<const char*>(Bytes.Data()), Bytes.Size());

                auto Loaded = std::make_unique<Scene>("", _Context);
                SceneSerializer::LoadFromString(*Loaded, Text);
                FinishSceneLoad(std::move(Loaded));

                return;
            }

            case PendingKind::LoadFile: {
                auto Loaded = std::make_unique<Scene>("", _Context);
                SceneSerializer::LoadFromFile(*Loaded, Path);
                FinishSceneLoad(std::move(Loaded));

                return;
            }
        }
    }

    void Game::TearDownActiveScene() {
        if (!_ActiveScene) return;

        OnSceneUnloading(*_ActiveScene);

        _ActiveScene->EndPlay();
        _ActiveScene.reset();
        _Textures->Clear();
        _Meshes->Clear();
    }

    void Game::FinishSceneLoad(std::unique_ptr<Scene> Loaded) {
        _ActiveScene = std::move(Loaded);
        PreloadSceneAssets(*_ActiveScene);
        OnSceneLoaded(*_ActiveScene);
        _ActiveScene->BeginPlay();
        _Accumulator = 0.0f;
    }
}  // namespace Xen