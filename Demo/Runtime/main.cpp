#define NOMINMAX

#include <XENGINE/Engine/Window.hpp>
#include <XENGINE/Engine/Game.hpp>

using namespace x;

class GameWindow final : public IWindow {
public:
    GameWindow() : IWindow("Demo", 1280, 720, 0), mGame(mContext) {
        AddListener(&mGame);
        SetOpenMaximized(true);
    }

    void OnInitialize() override {
        mGame.Initialize(this, mWindowViewport.get());
        // mGame.TransitionScene("");
    }

    void OnUpdate() override {
        mGame.Update();
    }

    void OnRender() override {
        mWindowViewport->AttachViewport();
        mWindowViewport->ClearAll();
        mGame.RenderFrame();
    }

private:
    Game mGame;
};

X_MAIN {
    GameWindow game;
    return game.Run();
}