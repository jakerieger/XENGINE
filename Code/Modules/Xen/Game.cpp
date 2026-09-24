//
// Created by Jake Rieger on 9/8/2026.
//

#include <Common/Log.hpp>

#include "Game.hpp"

#include <thread>
#include "Components/AntiAliasingComponent.hpp"
#include "AssetPreloader.hpp"
#include "SceneSerializer.hpp"
#include "AssetSettings.hpp"

#include <XenPAK/LooseFileSource.hpp>

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

        Config.EngineShaderSourceDir = Settings.EngineShaderSourceDir;
        Config.EngineShaderOutputDir = Settings.EngineShaderOutputDir;
        Config.EngineDxcPath         = Settings.EngineDxcPath;

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

        // Not fatal: without it a load just has no default screen. Painting
        // one background frame right away means the launch never shows an
        // unpainted window while the rest of this constructor runs.
        if (_LoadingScreen.Initialize(*_RenderDevice)) {
            _RenderDevice->BeginFrame();
            _LoadingScreen.DrawBackground();
            _RenderDevice->EndFrame();
        } else {
            LOG_WARN("loading screen unavailable - a load will show no default screen");
        }

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

        // Not fatal either: no FXAA shader asset just means no anti-aliasing,
        // same convention as _MeshRenderer above.
        if (!_FXAA.Initialize(*_RenderDevice, *_Assets, _MainViewport.GetColorFormat())) {
            LOG_DBG("FXAA not initialized (no shader asset found) - anti-aliasing unavailable");
        }

        // Dev-only: a no-op call in Release (MountConfig's two shader paths
        // are empty there - see AssetSettings.hpp). The loose override has
        // to be mounted before anything above already loaded a shader would
        // matter for a REPEAT load, but since this is the very first one,
        // mounting it here (rather than earlier, before _MeshRenderer/_FXAA
        // Initialize) makes no practical difference - nothing's been edited
        // yet at process start regardless.
        if (_ShaderHotReload.Initialize(
              MountConfig.EngineShaderSourceDir, MountConfig.EngineShaderOutputDir, MountConfig.EngineDxcPath)) {
            _Assets->AddSource(std::make_unique<PAK::LooseFileSource>(MountConfig.EngineShaderOutputDir,
                                                                       PAK::MOUNT_PRIORITY_LOOSE_BASE * 10));
        }
    }

    void Game::ReloadShaders() {
        LOG_INFO("Shader hot-reload: reloading render pipelines...");

        _MeshRenderer.Shutdown();
        if (!_MeshRenderer.Initialize(*_RenderDevice, *_Assets, _MainViewport)) {
            LOG_WARN("Shader hot-reload: mesh renderer failed to reinitialize - 3D rendering now unavailable");
        }

        _FXAA.Shutdown();
        if (!_FXAA.Initialize(*_RenderDevice, *_Assets, _MainViewport.GetColorFormat())) {
            LOG_WARN("Shader hot-reload: FXAA failed to reinitialize - anti-aliasing now unavailable");
        }
    }

    Game::~Game() {
        // First: the loader's workers use the caches and the device.
        _Load.reset();
        if (_ActiveScene) TearDownActiveScene();

        // Explicit, so the renderer and viewport release their GPU resources
        // while the device is unambiguously alive. The declaration order in
        // Game.hpp would get this right anyway; doing it here makes the
        // dependency visible instead of implicit.
        _Window->SetDebugUI(nullptr);
        _DebugUI.Shutdown();
        _SpriteRenderer.Shutdown();
        _LoadingScreen.Shutdown();
        _MeshRenderer.Shutdown();
        _FXAA.Shutdown();
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

        // A frame budget is for the game, not for waiting on its assets.
        while (_Running && _Load) {
            TickFrame(_FixedTimeStep);
        }

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

        // Dev-only, permanently a no-op in Release (see ShaderHotReload.hpp)
        // - checked before any BeginFrame below, since reloading a pipeline
        // needs to run outside one (the same constraint MeshRenderer::
        // Initialize's synchronous BRDF LUT bake already has).
        if (_ShaderHotReload.Poll(DeltaTime)) { ReloadShaders(); }

        // A scene is loading: there's nothing to update or draw but the
        // loading screen. RunLoop still pumps the window every iteration, so it
        // stays responsive.
        if (_Load) {
            TickLoading(DeltaTime);
            _Window->ResetInput();
            ApplyPendingSceneChange();
            return;
        }

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

            // Runs after sprites, on top of them - a no-op if this game has
            // no PBR shader asset (see the constructor). MeshRenderer draws
            // into its own offscreen HDR target and composites that onto
            // _MainViewport's color target last (exposure, bloom, tonemap -
            // see PostProcess.hpp), blended so only the pixels it actually
            // covered overwrite what the sprite pass above already drew.
            if (_MeshRenderer.IsInitialized()) _MeshRenderer.Render(*_ActiveScene, _MainViewport, DeltaTime);

            // Last: smooths whatever's left in the fully composited frame -
            // see FXAA.hpp for why this runs at the Game level rather than
            // inside MeshRenderer/PostProcess. Settings come from the
            // scene's first AntiAliasingComponent, the same "first one
            // found" rule as PostProcessComponent; a scene with none uses
            // Technique's own default (TAA). Only run when that component
            // picked FXAA specifically - TAA (if picked) already ran inside
            // MeshRenderer::Render, before the composite, since it needs the
            // linear-HDR scene color and motion vectors PostProcess consumes
            // before this point ever sees anything (see TAA.hpp); running
            // FXAA on top of an already-TAA'd frame would just soften it
            // further for no benefit.
            AntiAliasingTechnique AaTechnique = AntiAliasingTechnique::TAA;
            FXAA::Settings AaSettings;
            const std::vector<Actor*> AaActors = _ActiveScene->FindActorsWith<AntiAliasingComponent>();
            if (!AaActors.empty()) {
                if (const auto* AA = AaActors.front()->GetComponent<AntiAliasingComponent>()) {
                    AaTechnique = AA->GetTechnique();
                    AaSettings  = AA->GetFxaaSettings();
                }
            }
            AaSettings.Enabled &= AaTechnique == AntiAliasingTechnique::FXAA;
            const RHI::TextureHandle PresentTarget =
              _FXAA.Render(_MainViewport.GetColorTarget(), _MainViewport.GetWidth(), _MainViewport.GetHeight(), AaSettings);

            // Standalone-game presentation: copy the (possibly FXAA'd)
            // viewport color target into the back buffer. An editor
            // wouldn't call this at all - it would sample
            // _MainViewport.GetColorTarget() into an ImGui panel instead
            // (unaffected by FXAA, which never writes back into that
            // texture - see FXAA::Render's own comment).
            _RenderDevice->CopyToSwapChain(PresentTarget);

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

        CancelLoad();
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
                BeginSceneLoad(std::move(Loaded));

                return;
            }

            case PendingKind::LoadFile: {
                auto Loaded = std::make_unique<Scene>("", _Context);
                SceneSerializer::LoadFromFile(*Loaded, Path);
                BeginSceneLoad(std::move(Loaded));

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
        // Every asset is already resident (AssetLoader), so each component's
        // Acquire in BeginPlay is a cache hit.
        _ActiveScene = std::move(Loaded);
        OnSceneLoaded(*_ActiveScene);
        _ActiveScene->BeginPlay();
        _Accumulator = 0.0f;
    }

    void Game::BeginSceneLoad(std::unique_ptr<Scene> Loaded) {
        _Load           = std::make_unique<LoadState>();
        _Load->Incoming = std::move(Loaded);
        _Load->Start    = std::chrono::steady_clock::now();

        // Workers start unpacking and decoding right here; nothing is drawn
        // until TickLoading decides the load is slow enough to warrant it.
        _Load->Loader.Begin(GatherSceneLoadRequests(*_Load->Incoming), _Textures.get(), _Meshes.get());
        _Accumulator = 0.0f;
    }

    void Game::CancelLoad() {
        if (!_Load) return;

        _Load.reset();  // ~AssetLoader stops and joins the workers

        // Whatever the load had already made resident belongs to no scene.
        _Textures->Clear();
        _Meshes->Clear();
    }

    void Game::TickLoading(const f32 DeltaTime) {
        using Clock = std::chrono::steady_clock;

        if (_Window->ConsumeResized()) { SetViewport(_Window->GetWidth(), _Window->GetHeight()); }

        LoadState& L = *_Load;

        // The GPU half of every asset the workers have finished. A worker's
        // failure (a missing or undecodable asset) surfaces here, on the main
        // thread, where the old synchronous load threw it.
        try {
            L.AssetsDone = L.Loader.Pump(4.0);
        } catch (...) {
            _Load.reset();
            throw;
        }

        const auto Now = Clock::now();

        LoadingProgress Progress;
        Progress.Total          = L.Loader.Total();
        Progress.Done           = L.Loader.Done();
        Progress.Fraction       = (L.AssetsDone || Progress.Total == 0)
                                    ? 1.0f
                                    : CAST<f32>(Progress.Done) / CAST<f32>(Progress.Total);
        Progress.ElapsedSeconds = std::chrono::duration<f32>(Now - L.Start).count();

        const LoadingScreen::Config& Cfg = _LoadingScreen.GetConfig();
        if (!L.Visible && Progress.ElapsedSeconds >= Cfg.ShowDelaySeconds) {
            L.Visible      = true;
            L.VisibleSince = Now;
            _LoadingScreen.Reset();
        }

        const bool CanPresent = !_Window->IsMinimized();
        if (L.Visible && CanPresent) {
            DrawLoadingFrame(Progress, DeltaTime, false);
        } else {
            // Nothing is being presented (so nothing paces this loop the way
            // vsync does): don't spin a core while the workers decode.
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        if (!L.AssetsDone) return;

        // Once it has appeared, keep it up for its minimum time.
        if (L.Visible && std::chrono::duration<f32>(Now - L.VisibleSince).count() < Cfg.MinVisibleSeconds) return;

        const bool WasVisible = L.Visible;
        std::unique_ptr<Scene> Incoming = std::move(L.Incoming);
        _Load.reset();  // L is gone from here on

        FinishSceneLoad(std::move(Incoming));

        // If the loading screen is up, bake the environment under it instead
        // of hitching the first real frame.
        if (WasVisible && CanPresent && _MeshRenderer.IsInitialized()) {
            DrawLoadingFrame(Progress, 0.0f, true);
        }
    }

    void Game::DrawLoadingFrame(const LoadingProgress& Progress, const f32 DeltaTime, const bool WarmUpEnvironment) {
        _RenderDevice->BeginFrame();

        if (WarmUpEnvironment && _ActiveScene) _MeshRenderer.PrepareEnvironment(*_ActiveScene);

        if (!OnLoadingScreen(Progress) && _LoadingScreen.IsInitialized()) { _LoadingScreen.Draw(Progress, DeltaTime); }

        _RenderDevice->EndFrame();
    }
}  // namespace Xen