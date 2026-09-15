//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include "AssetSettings.hpp"
#include "EngineCommon.hpp"
#include "EngineConfig.hpp"
#include "Scene.hpp"
#include "SpriteBatcher.hpp"
#include "SpriteRenderer.hpp"
#include "Window.hpp"

#include <PAK/AssetMount.hpp>
#include <PAK/AssetRegistry.hpp>

#include <filesystem>

namespace Xen {
    /// @brief Root object. Owns engine services, the active scene, and the
    /// main loop.
    ///
    /// Subclass it and override the On* hooks for game-specific behavior:
    ///
    ///     ```cpp
    ///     class MyGame final : public Game {
    ///         using Game::Game;
    ///         void OnStartup() override { LoadSceneFromFile("scenes/level1.json"); }
    ///         void OnUpdate(f32 Dt) override { ... }
    ///     };
    ///
    ///     MyGame G(Config{}, Assets);
    ///     G.Run();
    ///     ```
    class Game {
    public:
        Game(const std::string& Title, const PAK::AssetMountConfig& MountConfig);
        virtual ~Game();

        Game(const Game&)            = delete;
        Game& operator=(const Game&) = delete;

        void Run();
        void RunFrames(u32 FrameCount);
        void Quit();

        _NoDiscard bool IsRunning() const { return _Running; };

        void LoadScene(AssetID SceneAsset);
        void LoadSceneFromFile(std::filesystem::path Path);
        void UnloadScene();

        _NoDiscard bool IsSceneChangePending() const { return _PendingSceneChange; };
        _NoDiscard Scene* GetActiveScene() const { return _ActiveScene.get(); };

        _NoDiscard const EngineContext& GetContext() const { return _Context; }
        _NoDiscard TextureCache& GetTextures() const { return *_Textures; }
        _NoDiscard SpriteBatcher& GetBatcher() { return _SpriteBatcher; }
        _NoDiscard SpriteRenderer& GetRenderer() { return _SpriteRenderer; }
        _NoDiscard RHI::IRenderDevice& GetRenderDevice() const { return *_RenderDevice; }
        _NoDiscard Window& GetWindow() const { return *_Window; }

        _NoDiscard u64 GetFrameCount() const { return _FrameCount; }
        _NoDiscard f32 GetLastFrameDelta() const { return _LastDelta; }

        void SetFixedTimeStep(const f32 Step) {
            if (Step > 0.0f) _FixedTimeStep = Step;
        }

        /// @brief How far into the next fixed step this frame is rendering,
        /// in [0, 1).
        ///
        /// Fixed-step simulation and variable-rate rendering do not line up:
        /// at 144Hz with a 60Hz step, two out of every five frames draw an
        /// unchanged position. That reads as stutter even though the
        /// simulation is perfectly smooth. Interpolating rendered transforms
        /// between the previous and current fixed state by this value is the
        /// fix - see the note below.
        _NoDiscard f32 GetFixedAlpha() const { return _FixedTimeStep > 0.0f ? _Accumulator / _FixedTimeStep : 0.0f; }

        /// @brief Pushes a framebuffer size to everything that needs one.
        ///
        /// Both halves matter. The device sizes the swap chain viewport from
        /// it - miss that and the viewport stays 0x0 and nothing rasterizes.
        /// The batcher feeds it to the active camera, which is what decides
        /// how much world fits on screen.
        void SetViewport(u32 Width, u32 Height);

    protected:
        // --- Lifecycle hooks (override these) ---------------------------

        /// @brief After services are up, before the first frame. Load the
        /// first scene here.
        virtual void OnStartup() {}

        /// @brief After the loop ends and the scene is torn down.
        virtual void OnShutdown() {}

        /// @brief After a scene is loaded and its assets are resident, but
        /// BEFORE BeginPlay - so game code can inject actors or wire
        /// references that should exist from the scene's first frame.
        virtual void OnSceneLoaded(Scene& S) { (void)S; }

        /// @brief Before a scene is torn down, while its actors are still
        /// alive and inspectable.
        virtual void OnSceneUnloading(Scene& S) { (void)S; }

        /// @brief Fixed-step update. May run zero or several times per frame.
        /// Physics and anything needing determinism belongs here.
        virtual void OnFixedUpdate(const f32 FixedDelta) { (void)FixedDelta; }

        /// @brief Once per frame with the real elapsed time. Camera smoothing,
        /// input polling, anything framerate-dependent.
        virtual void OnUpdate(const f32 DeltaTime) { (void)DeltaTime; }

        /// @brief After the draw list is built, before it is submitted.
        virtual void OnRender() {}

    private:
        enum class PendingKind : u8 { None, LoadAsset, LoadFile, Unload };

        void RunLoop();
        void TickFrame(f32 DeltaTime);
        void ApplyPendingSceneChange();
        void TearDownActiveScene();
        void FinishSceneLoad(std::unique_ptr<Scene> Loaded);

        EngineConfig _EngineConfig {};
        AudioConfig _AudioConfig {};

        std::unique_ptr<PAK::AssetRegistry> _Assets;
        std::unique_ptr<TextureCache> _Textures;
        EngineContext _Context {};
        SpriteBatcher _SpriteBatcher;
        std::unique_ptr<Window> _Window;
        std::unique_ptr<RHI::IRenderDevice> _RenderDevice;
        SpriteRenderer _SpriteRenderer;

        std::unique_ptr<Scene> _ActiveScene;

        bool _PendingSceneChange {false};
        PendingKind _PendingKind {PendingKind::None};
        AssetID _PendingAsset {};
        std::filesystem::path _PendingPath {};

        bool _Running {false};
        f32 _Accumulator {0.0f};
        f32 _LastDelta {0.0f};
        u64 _FrameCount {0};
        f32 _FixedTimeStep {1.0f / 60.0f};
        u32 _MaxFixedStepsPerFrame {5};
        f32 _MaxFrameDelta {0.25f};
    };

    template<typename GameClass>
    void RunGame(const std::string& Name, const AssetSettings& Settings, const int argc, char* argv[]) noexcept {
        _AssertBaseOf(Game, GameClass);
        const auto MountConfig = BuildMountConfig(Settings, argc, argv);

        try {
            GameClass {Name, MountConfig}.Run();
        } catch (const EngineException& Ex) {
            std::fprintf(stderr, "%s\n", Ex.what());
            std::exit(1);
        }
    }
}  // namespace Xen

#ifdef PLATFORM_WINDOWS
    #ifndef _WINDOWS_
        #define WIN32_LEAN_AND_MEAN
        #define NOMINMAX
        #include <Windows.h>
    #endif

    #define XEN_ENTRYPOINT int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)

    #define XEN_GAME(GameClass, Title)                                                                                 \
        XEN_ENTRYPOINT {                                                                                               \
            Xen::RunGame<GameClass>(Title, Xen::Generated::GameSettings(), __argc, __argv);                            \
            return 0;                                                                                                  \
        }
#else
    #define XEN_ENTRYPOINT int main(int argc, char** argv)

    #define XEN_GAME(GameClass, Title)                                                                                 \
        XEN_ENTRYPOINT {                                                                                               \
            Xen::RunGame<GameClass>(Title, Xen::Generated::GameSettings(), argc, argv);                                \
        }
#endif
