//
// Created by Jake Rieger on 9/15/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include "EngineConfig.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <unordered_map>

namespace Xen {
    namespace Input::KeyCode {
        constexpr i16 Unknown      = GLFW_KEY_UNKNOWN;
        constexpr i16 Space        = GLFW_KEY_SPACE;
        constexpr i16 Apostrophe   = GLFW_KEY_APOSTROPHE;
        constexpr i16 Comma        = GLFW_KEY_COMMA;
        constexpr i16 Minus        = GLFW_KEY_MINUS;
        constexpr i16 Period       = GLFW_KEY_PERIOD;
        constexpr i16 Slash        = GLFW_KEY_SLASH;
        constexpr i16 Num0         = GLFW_KEY_0;
        constexpr i16 Num1         = GLFW_KEY_1;
        constexpr i16 Num2         = GLFW_KEY_2;
        constexpr i16 Num3         = GLFW_KEY_3;
        constexpr i16 Num4         = GLFW_KEY_4;
        constexpr i16 Num5         = GLFW_KEY_5;
        constexpr i16 Num6         = GLFW_KEY_6;
        constexpr i16 Num7         = GLFW_KEY_7;
        constexpr i16 Num8         = GLFW_KEY_8;
        constexpr i16 Num9         = GLFW_KEY_9;
        constexpr i16 Semicolon    = GLFW_KEY_SEMICOLON;
        constexpr i16 Equal        = GLFW_KEY_EQUAL;
        constexpr i16 A            = GLFW_KEY_A;
        constexpr i16 B            = GLFW_KEY_B;
        constexpr i16 C            = GLFW_KEY_C;
        constexpr i16 D            = GLFW_KEY_D;
        constexpr i16 E            = GLFW_KEY_E;
        constexpr i16 F            = GLFW_KEY_F;
        constexpr i16 G            = GLFW_KEY_G;
        constexpr i16 H            = GLFW_KEY_H;
        constexpr i16 I            = GLFW_KEY_I;
        constexpr i16 J            = GLFW_KEY_J;
        constexpr i16 K            = GLFW_KEY_K;
        constexpr i16 L            = GLFW_KEY_L;
        constexpr i16 M            = GLFW_KEY_M;
        constexpr i16 N            = GLFW_KEY_N;
        constexpr i16 O            = GLFW_KEY_O;
        constexpr i16 P            = GLFW_KEY_P;
        constexpr i16 Q            = GLFW_KEY_Q;
        constexpr i16 R            = GLFW_KEY_R;
        constexpr i16 S            = GLFW_KEY_S;
        constexpr i16 T            = GLFW_KEY_T;
        constexpr i16 U            = GLFW_KEY_U;
        constexpr i16 V            = GLFW_KEY_V;
        constexpr i16 W            = GLFW_KEY_W;
        constexpr i16 X            = GLFW_KEY_X;
        constexpr i16 Y            = GLFW_KEY_Y;
        constexpr i16 Z            = GLFW_KEY_Z;
        constexpr i16 LeftBracket  = GLFW_KEY_LEFT_BRACKET;
        constexpr i16 Backslash    = GLFW_KEY_BACKSLASH;
        constexpr i16 RightBracket = GLFW_KEY_RIGHT_BRACKET;
        constexpr i16 GraveAccent  = GLFW_KEY_GRAVE_ACCENT;
        constexpr i16 World1       = GLFW_KEY_WORLD_1;
        constexpr i16 World2       = GLFW_KEY_WORLD_2;
        constexpr i16 Escape       = GLFW_KEY_ESCAPE;
        constexpr i16 Enter        = GLFW_KEY_ENTER;
        constexpr i16 Tab          = GLFW_KEY_TAB;
        constexpr i16 Backspace    = GLFW_KEY_BACKSPACE;
        constexpr i16 Insert       = GLFW_KEY_INSERT;
        constexpr i16 Delete       = GLFW_KEY_DELETE;
        constexpr i16 Right        = GLFW_KEY_RIGHT;
        constexpr i16 Left         = GLFW_KEY_LEFT;
        constexpr i16 Down         = GLFW_KEY_DOWN;
        constexpr i16 Up           = GLFW_KEY_UP;
        constexpr i16 PageUp       = GLFW_KEY_PAGE_UP;
        constexpr i16 PageDown     = GLFW_KEY_PAGE_DOWN;
        constexpr i16 Home         = GLFW_KEY_HOME;
        constexpr i16 End          = GLFW_KEY_END;
        constexpr i16 CapsLock     = GLFW_KEY_CAPS_LOCK;
        constexpr i16 ScrollLock   = GLFW_KEY_SCROLL_LOCK;
        constexpr i16 NumLock      = GLFW_KEY_NUM_LOCK;
        constexpr i16 PrintScreen  = GLFW_KEY_PRINT_SCREEN;
        constexpr i16 Pause        = GLFW_KEY_PAUSE;
        constexpr i16 F1           = GLFW_KEY_F1;
        constexpr i16 F2           = GLFW_KEY_F2;
        constexpr i16 F3           = GLFW_KEY_F3;
        constexpr i16 F4           = GLFW_KEY_F4;
        constexpr i16 F5           = GLFW_KEY_F5;
        constexpr i16 F6           = GLFW_KEY_F6;
        constexpr i16 F7           = GLFW_KEY_F7;
        constexpr i16 F8           = GLFW_KEY_F8;
        constexpr i16 F9           = GLFW_KEY_F9;
        constexpr i16 F10          = GLFW_KEY_F10;
        constexpr i16 F11          = GLFW_KEY_F11;
        constexpr i16 F12          = GLFW_KEY_F12;
        constexpr i16 F13          = GLFW_KEY_F13;
        constexpr i16 F14          = GLFW_KEY_F14;
        constexpr i16 F15          = GLFW_KEY_F15;
        constexpr i16 F16          = GLFW_KEY_F16;
        constexpr i16 F17          = GLFW_KEY_F17;
        constexpr i16 F18          = GLFW_KEY_F18;
        constexpr i16 F19          = GLFW_KEY_F19;
        constexpr i16 F20          = GLFW_KEY_F20;
        constexpr i16 F21          = GLFW_KEY_F21;
        constexpr i16 F22          = GLFW_KEY_F22;
        constexpr i16 F23          = GLFW_KEY_F23;
        constexpr i16 F24          = GLFW_KEY_F24;
        constexpr i16 F25          = GLFW_KEY_F25;
        constexpr i16 Kp0          = GLFW_KEY_KP_0;
        constexpr i16 Kp1          = GLFW_KEY_KP_1;
        constexpr i16 Kp2          = GLFW_KEY_KP_2;
        constexpr i16 Kp3          = GLFW_KEY_KP_3;
        constexpr i16 Kp4          = GLFW_KEY_KP_4;
        constexpr i16 Kp5          = GLFW_KEY_KP_5;
        constexpr i16 Kp6          = GLFW_KEY_KP_6;
        constexpr i16 Kp7          = GLFW_KEY_KP_7;
        constexpr i16 Kp8          = GLFW_KEY_KP_8;
        constexpr i16 Kp9          = GLFW_KEY_KP_9;
        constexpr i16 KpDecimal    = GLFW_KEY_KP_DECIMAL;
        constexpr i16 KpDivide     = GLFW_KEY_KP_DIVIDE;
        constexpr i16 KpMultiply   = GLFW_KEY_KP_MULTIPLY;
        constexpr i16 KpSubtract   = GLFW_KEY_KP_SUBTRACT;
        constexpr i16 KpAdd        = GLFW_KEY_KP_ADD;
        constexpr i16 KpEnter      = GLFW_KEY_KP_ENTER;
        constexpr i16 KpEqual      = GLFW_KEY_KP_EQUAL;
        constexpr i16 LeftShift    = GLFW_KEY_LEFT_SHIFT;
        constexpr i16 LeftControl  = GLFW_KEY_LEFT_CONTROL;
        constexpr i16 LeftAlt      = GLFW_KEY_LEFT_ALT;
        constexpr i16 LeftSuper    = GLFW_KEY_LEFT_SUPER;
        constexpr i16 RightShift   = GLFW_KEY_RIGHT_SHIFT;
        constexpr i16 RightControl = GLFW_KEY_RIGHT_CONTROL;
        constexpr i16 RightAlt     = GLFW_KEY_RIGHT_ALT;
        constexpr i16 RightSuper   = GLFW_KEY_RIGHT_SUPER;
        constexpr i16 Menu         = GLFW_KEY_MENU;
        constexpr i16 Last         = GLFW_KEY_LAST;

        inline static const std::unordered_map<std::string, i16> KeyCodeMap = {
          {"Space", KeyCode::Space},
          {"Apostrophe", KeyCode::Apostrophe},
          {"Comma", KeyCode::Comma},
          {"Minus", KeyCode::Minus},
          {"Period", KeyCode::Period},
          {"Slash", KeyCode::Slash},
          {"Num0", KeyCode::Num0},
          {"Num1", KeyCode::Num1},
          {"Num2", KeyCode::Num2},
          {"Num3", KeyCode::Num3},
          {"Num4", KeyCode::Num4},
          {"Num5", KeyCode::Num5},
          {"Num6", KeyCode::Num6},
          {"Num7", KeyCode::Num7},
          {"Num8", KeyCode::Num8},
          {"Num9", KeyCode::Num9},
          {"Semicolon", KeyCode::Semicolon},
          {"Equal", KeyCode::Equal},
          {"A", KeyCode::A},
          {"B", KeyCode::B},
          {"C", KeyCode::C},
          {"D", KeyCode::D},
          {"E", KeyCode::E},
          {"F", KeyCode::F},
          {"G", KeyCode::G},
          {"H", KeyCode::H},
          {"I", KeyCode::I},
          {"J", KeyCode::J},
          {"K", KeyCode::K},
          {"L", KeyCode::L},
          {"M", KeyCode::M},
          {"N", KeyCode::N},
          {"O", KeyCode::O},
          {"P", KeyCode::P},
          {"Q", KeyCode::Q},
          {"R", KeyCode::R},
          {"S", KeyCode::S},
          {"T", KeyCode::T},
          {"U", KeyCode::U},
          {"V", KeyCode::V},
          {"W", KeyCode::W},
          {"X", KeyCode::X},
          {"Y", KeyCode::Y},
          {"Z", KeyCode::Z},
          {"LeftBracket", KeyCode::LeftBracket},
          {"Backslash", KeyCode::Backslash},
          {"RightBracket", KeyCode::RightBracket},
          {"GraveAccent", KeyCode::GraveAccent},
          {"World1", KeyCode::World1},
          {"World2", KeyCode::World2},
          {"Escape", KeyCode::Escape},
          {"Enter", KeyCode::Enter},
          {"Tab", KeyCode::Tab},
          {"Backspace", KeyCode::Backspace},
          {"Insert", KeyCode::Insert},
          {"Delete", KeyCode::Delete},
          {"Right", KeyCode::Right},
          {"Left", KeyCode::Left},
          {"Down", KeyCode::Down},
          {"Up", KeyCode::Up},
          {"PageUp", KeyCode::PageUp},
          {"PageDown", KeyCode::PageDown},
          {"Home", KeyCode::Home},
          {"End", KeyCode::End},
          {"CapsLock", KeyCode::CapsLock},
          {"ScrollLock", KeyCode::ScrollLock},
          {"NumLock", KeyCode::NumLock},
          {"PrintScreen", KeyCode::PrintScreen},
          {"Pause", KeyCode::Pause},
          {"F1", KeyCode::F1},
          {"F2", KeyCode::F2},
          {"F3", KeyCode::F3},
          {"F4", KeyCode::F4},
          {"F5", KeyCode::F5},
          {"F6", KeyCode::F6},
          {"F7", KeyCode::F7},
          {"F8", KeyCode::F8},
          {"F9", KeyCode::F9},
          {"F10", KeyCode::F10},
          {"F11", KeyCode::F11},
          {"F12", KeyCode::F12},
          {"F13", KeyCode::F13},
          {"F14", KeyCode::F14},
          {"F15", KeyCode::F15},
          {"F16", KeyCode::F16},
          {"F17", KeyCode::F17},
          {"F18", KeyCode::F18},
          {"F19", KeyCode::F19},
          {"F20", KeyCode::F20},
          {"F21", KeyCode::F21},
          {"F22", KeyCode::F22},
          {"F23", KeyCode::F23},
          {"F24", KeyCode::F24},
          {"F25", KeyCode::F25},
          {"KP0", KeyCode::Kp0},
          {"KP1", KeyCode::Kp1},
          {"KP2", KeyCode::Kp2},
          {"KP3", KeyCode::Kp3},
          {"KP4", KeyCode::Kp4},
          {"KP5", KeyCode::Kp5},
          {"KP6", KeyCode::Kp6},
          {"KP7", KeyCode::Kp7},
          {"KP8", KeyCode::Kp8},
          {"KP9", KeyCode::Kp9},
          {"KPDecimal", KeyCode::KpDecimal},
          {"KPDivide", KeyCode::KpDivide},
          {"KPMultiply", KeyCode::KpMultiply},
          {"KPSubtract", KeyCode::KpSubtract},
          {"KPAdd", KeyCode::KpAdd},
          {"KPEnter", KeyCode::KpEnter},
          {"KPEqual", KeyCode::KpEqual},
          {"LeftBracket", KeyCode::LeftBracket},
          {"RightBracket", KeyCode::RightBracket},
          {"LeftShift", KeyCode::LeftShift},
          {"LeftControl", KeyCode::LeftControl},
          {"LeftAlt", KeyCode::LeftAlt},
          {"LeftSuper", KeyCode::LeftSuper},
          {"RightShift", KeyCode::RightShift},
          {"RightControl", KeyCode::RightControl},
          {"RightAlt", KeyCode::RightAlt},
          {"RightSuper", KeyCode::RightSuper},
          {"Menu", KeyCode::Menu},
        };
    }  // namespace Input::KeyCode

    namespace Input::MouseButton {
        constexpr i16 Button1 = GLFW_MOUSE_BUTTON_1;
        constexpr i16 Button2 = GLFW_MOUSE_BUTTON_2;
        constexpr i16 Button3 = GLFW_MOUSE_BUTTON_3;
        constexpr i16 Button4 = GLFW_MOUSE_BUTTON_4;
        constexpr i16 Button5 = GLFW_MOUSE_BUTTON_5;
        constexpr i16 Button6 = GLFW_MOUSE_BUTTON_6;
        constexpr i16 Button7 = GLFW_MOUSE_BUTTON_7;
        constexpr i16 Button8 = GLFW_MOUSE_BUTTON_8;
        constexpr i16 Left    = GLFW_MOUSE_BUTTON_LEFT;
        constexpr i16 Right   = GLFW_MOUSE_BUTTON_RIGHT;
        constexpr i16 Middle  = GLFW_MOUSE_BUTTON_MIDDLE;
        constexpr i16 Last    = GLFW_MOUSE_BUTTON_LAST;

        inline static const std::unordered_map<std::string, i16> MouseButtonMap = {
          {"Left", MouseButton::Left},
          {"Right", MouseButton::Right},
          {"Middle", MouseButton::Middle},
          {"Button1", MouseButton::Button1},
          {"Button2", MouseButton::Button2},
          {"Button3", MouseButton::Button3},
          {"Button4", MouseButton::Button4},
          {"Button5", MouseButton::Button5},
          {"Button6", MouseButton::Button6},
          {"Button7", MouseButton::Button7},
          {"Button8", MouseButton::Button8},
          {"Last", MouseButton::Last},
        };
    }  // namespace Input::MouseButton

    class InputMap {
        InputMap(const InputMap&)            = delete;
        InputMap(InputMap&&)                 = delete;
        InputMap& operator=(const InputMap&) = delete;
        InputMap& operator=(InputMap&&)      = delete;

    public:
        struct Action {
            std::vector<i16> KeyCodes;
            std::vector<i16> MouseButtons;
        };

        InputMap() = default;

        void Load(const std::filesystem::path& InputConfig) {
            if (!exists(InputConfig)) {
                THROW_ENGINE_EXCEPTION(EngineException,
                                       "failed to load input config (missing: '" + InputConfig.string() + "')");
            }

            Config::Ini Cfg = Config::Read(InputConfig.string());

            for (const auto& [Name, Bindings] : Cfg["Actions"]) {
                const auto ActionList = Config::GetList(Bindings);
                Action Map;
                for (const auto& Binding : ActionList) {
                    if (Binding.find("Key.") != std::string::npos) {
                        const auto KeyStr = Binding.substr(4, Binding.length());
                        const i16 KeyCode = Input::KeyCode::KeyCodeMap.at(KeyStr);
                        Map.KeyCodes.push_back(KeyCode);
                    }

                    if (Binding.find("Mouse.") != std::string::npos) {
                        const auto MouseStr = Binding.substr(6, Binding.length());
                        const i16 MouseCode = Input::MouseButton::MouseButtonMap.at(MouseStr);
                        Map.MouseButtons.push_back(MouseCode);
                    }
                }

                _Actions[Name] = Map;
            }

            _Loaded = true;
        }

        bool IsLoaded() const { return _Loaded; }
        std::unordered_map<std::string, Action> const& GetActions() const { return _Actions; }

    private:
        bool _Loaded {false};
        std::unordered_map<std::string, Action> _Actions;
    };

    class InputManager {
        friend class Window;

        InputManager(const InputManager&)            = delete;
        InputManager(InputManager&&)                 = delete;
        InputManager& operator=(const InputManager&) = delete;
        InputManager& operator=(InputManager&&)      = delete;

    public:
        bool GetKeyDown(const i16 Key) { return _KeyStates[Key]; }
        bool GetKeyUp(const i16 Key) { return !_KeyStates[Key]; }
        bool GetMouseButtonDown(const i16 Button) { return _MouseButtonStates[Button]; }
        bool GetMouseButtonUp(const i16 Button) { return !_MouseButtonStates[Button]; }

        bool GetAction(const std::string& Name) {
            if (!_InputMap.IsLoaded() || _InputMap.GetActions().contains(Name)) return false;

            const auto [KeyCodes, MouseButtons] = _InputMap.GetActions().at(Name);
            const bool KeyDown = std::ranges::any_of(KeyCodes, [this](const i16 Key) { return GetKeyDown(Key); });
            const bool MouseDown =
              std::ranges::any_of(MouseButtons, [this](const i16 Button) { return GetMouseButtonDown(Button); });

            return KeyDown || MouseDown;
        }

        NODISCARD i32 GetMouseX() const { return _MouseX; }
        NODISCARD i32 GetMouseY() const { return _MouseY; }
        NODISCARD i32 GetMouseDeltaX() const { return _MouseDeltaX; }
        NODISCARD i32 GetMouseDeltaY() const { return _MouseDeltaY; }

    private:
        InputManager() = default;

        InputMap _InputMap;
        std::unordered_map<i16, bool> _KeyStates;
        std::unordered_map<i16, bool> _MouseButtonStates;
        i32 _MouseX, _MouseY;
        i32 _MouseDeltaX, _MouseDeltaY;
        bool _Enabled {true};

        void LoadInputMap(const std::filesystem::path& InputConfig) { _InputMap.Load(InputConfig); }

        void SetEnabled(const bool Enabled) { _Enabled = Enabled; }

        void UpdateKeyState(const i16 KeyCode, const bool Pressed) {
            if (!_Enabled) return;
            _KeyStates[KeyCode] = Pressed;
        }

        void UpdateMouseButtonState(const i16 Button, const bool Pressed) {
            if (!_Enabled) return;
            _MouseButtonStates[Button] = Pressed;
        }

        void UpdateMousePosition(const i32 DeltaX, const i32 DeltaY) {
            if (!_Enabled) return;

            _MouseDeltaX = DeltaX;
            _MouseDeltaY = DeltaY;

            // TODO: Find a better way to do this, frame rate dependent (forces input checks to FixedTick)
            constexpr f32 DeadZone = 2.5f;
            if (std::abs(_MouseDeltaX) < DeadZone) _MouseDeltaX = 0.0f;
            if (std::abs(_MouseDeltaY) < DeadZone) _MouseDeltaY = 0.0f;

            _MouseX += DeltaX;
            _MouseY += DeltaY;
        }

        void ResetMouseDeltas() {
            _MouseDeltaX = 0;
            _MouseDeltaY = 0;
        }
    };
}  // namespace Xen