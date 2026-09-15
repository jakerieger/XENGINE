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
        Config.PakFiles = Settings.PakFiles;

        if (Settings.MountContentDirs) {
            Config.ContentDirs = Settings.ContentDirs;
            if (Settings.AllowCommandLineContentDirs) { PAK::AppendContentDirsFromArgs(Config, argc, argv); }
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
        } catch (const std::exception& Ex) {
            std::fprintf(stderr, "(warning) falling back to default config: %s\n", Ex.what());
        }

        _Window = std::make_unique<Window>(Title,
                                           _EngineConfig.WindowMode,
                                           _EngineConfig.ResolutionX,
                                           _EngineConfig.ResolutionY);

        _RenderDevice = RHI::CreateRenderDevice(RHI::Backend::OpenGL);
        if (!_RenderDevice) { _ThrowEngineException(EngineException, "no render device for the requested backend"); }

        RHI::DeviceDescriptor Descriptor {};
#ifndef NDEBUG
        Descriptor.EnableValidation   = true;
        Descriptor.EnableDebugMarkers = true;
#endif

        if (!_RenderDevice->Initialize(Descriptor)) {
            _ThrowEngineException(EngineException, "failed to initialize render device");
        }

        // Without this the device's swap chain size stays 0x0, BeginRenderPass
        // sets a 0x0 viewport, and every triangle is clipped away while the
        // clear still works - which looks exactly like a correct frame with no
        // content in it.
        SetViewport(_Window->GetWidth(), _Window->GetHeight());

        if (!_SpriteRenderer.Initialize(*_RenderDevice)) {
            _ThrowEngineException(EngineException, "failed to initialize sprite renderer");
        }

        _Assets = PAK::MountAssets(MountConfig);
        if (!_Assets) { _ThrowEngineException(EngineException, "failed to mount assets"); }
        _LogDebug("Asset mount configuration:\n%s", PAK::DescribeMounts(*_Assets).c_str());

        _Textures = std::make_unique<TextureCache>(*_Assets, *_RenderDevice);

        _Context.Assets   = _Assets.get();
        _Context.Owner    = this;
        _Context.Textures = _Textures.get();
    }

    Game::~Game() {
        if (_ActiveScene) TearDownActiveScene();

        // Explicit, so the renderer releases its pipeline and sampler while
        // the device is unambiguously alive. The declaration order in Game.hpp
        // would get this right anyway; doing it here makes the dependency
        // visible instead of implicit.
        _SpriteRenderer.Shutdown();
        _Textures.reset();
    }

    void Game::Run() {
        _Running = true;

        // Load startup scene
        if (!_EngineConfig.StartupScene.empty()) {
            const AssetID SceneAsset = ASSET(_EngineConfig.StartupScene.c_str());
            if (!SceneAsset.IsValid()) {
                _ThrowEngineException(EngineException,
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

            _Window->SwapBuffers();
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

            _SpriteBatcher.BuildDrawList(*_ActiveScene);
            OnRender();
            _SpriteRenderer.Render(_SpriteBatcher, *_Textures);

            _RenderDevice->EndFrame();
        }

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
                    _ThrowEngineException(EngineException, std::format("scene asset {} not found", Asset.Value));
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
    }

    void Game::FinishSceneLoad(std::unique_ptr<Scene> Loaded) {
        _ActiveScene = std::move(Loaded);
        PreloadSceneAssets(*_ActiveScene);
        OnSceneLoaded(*_ActiveScene);
        _ActiveScene->BeginPlay();
        _Accumulator = 0.0f;
    }
}  // namespace Xen