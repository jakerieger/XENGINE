//
// Created by Jake Rieger on 9/15/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include "EngineConfig.hpp"

#include <Windows.h>
#include <array>
#include <unordered_map>

namespace Xen {
    namespace Input::KeyCode {
        constexpr i16 Unknown      = -1;
        constexpr i16 Space        = VK_SPACE;
        constexpr i16 Apostrophe   = VK_OEM_7;
        constexpr i16 Comma        = VK_OEM_COMMA;
        constexpr i16 Minus        = VK_OEM_MINUS;
        constexpr i16 Period       = VK_OEM_PERIOD;
        constexpr i16 Slash        = VK_OEM_2;
        constexpr i16 Num0         = '0';
        constexpr i16 Num1         = '1';
        constexpr i16 Num2         = '2';
        constexpr i16 Num3         = '3';
        constexpr i16 Num4         = '4';
        constexpr i16 Num5         = '5';
        constexpr i16 Num6         = '6';
        constexpr i16 Num7         = '7';
        constexpr i16 Num8         = '8';
        constexpr i16 Num9         = '9';
        constexpr i16 Semicolon    = VK_OEM_1;
        constexpr i16 Equal        = VK_OEM_PLUS;
        constexpr i16 A            = 'A';
        constexpr i16 B            = 'B';
        constexpr i16 C            = 'C';
        constexpr i16 D            = 'D';
        constexpr i16 E            = 'E';
        constexpr i16 F            = 'F';
        constexpr i16 G            = 'G';
        constexpr i16 H            = 'H';
        constexpr i16 I            = 'I';
        constexpr i16 J            = 'J';
        constexpr i16 K            = 'K';
        constexpr i16 L            = 'L';
        constexpr i16 M            = 'M';
        constexpr i16 N            = 'N';
        constexpr i16 O            = 'O';
        constexpr i16 P            = 'P';
        constexpr i16 Q            = 'Q';
        constexpr i16 R            = 'R';
        constexpr i16 S            = 'S';
        constexpr i16 T            = 'T';
        constexpr i16 U            = 'U';
        constexpr i16 V            = 'V';
        constexpr i16 W            = 'W';
        constexpr i16 X            = 'X';
        constexpr i16 Y            = 'Y';
        constexpr i16 Z            = 'Z';
        constexpr i16 LeftBracket  = VK_OEM_4;
        constexpr i16 Backslash    = VK_OEM_5;
        constexpr i16 RightBracket = VK_OEM_6;
        constexpr i16 GraveAccent  = VK_OEM_3;
        constexpr i16 World1       = VK_OEM_8;    // No exact Win32 equivalent - closest "extra OEM key".
        constexpr i16 World2       = VK_OEM_102;  // Ditto.
        constexpr i16 Escape       = VK_ESCAPE;
        constexpr i16 Enter        = VK_RETURN;
        constexpr i16 Tab          = VK_TAB;
        constexpr i16 Backspace    = VK_BACK;
        constexpr i16 Insert       = VK_INSERT;
        constexpr i16 Delete       = VK_DELETE;
        constexpr i16 Right        = VK_RIGHT;
        constexpr i16 Left         = VK_LEFT;
        constexpr i16 Down         = VK_DOWN;
        constexpr i16 Up           = VK_UP;
        constexpr i16 PageUp       = VK_PRIOR;
        constexpr i16 PageDown     = VK_NEXT;
        constexpr i16 Home         = VK_HOME;
        constexpr i16 End          = VK_END;
        constexpr i16 CapsLock     = VK_CAPITAL;
        constexpr i16 ScrollLock   = VK_SCROLL;
        constexpr i16 NumLock      = VK_NUMLOCK;
        constexpr i16 PrintScreen  = VK_SNAPSHOT;
        constexpr i16 Pause        = VK_PAUSE;
        constexpr i16 F1           = VK_F1;
        constexpr i16 F2           = VK_F2;
        constexpr i16 F3           = VK_F3;
        constexpr i16 F4           = VK_F4;
        constexpr i16 F5           = VK_F5;
        constexpr i16 F6           = VK_F6;
        constexpr i16 F7           = VK_F7;
        constexpr i16 F8           = VK_F8;
        constexpr i16 F9           = VK_F9;
        constexpr i16 F10          = VK_F10;
        constexpr i16 F11          = VK_F11;
        constexpr i16 F12          = VK_F12;
        constexpr i16 F13          = VK_F13;
        constexpr i16 F14          = VK_F14;
        constexpr i16 F15          = VK_F15;
        constexpr i16 F16          = VK_F16;
        constexpr i16 F17          = VK_F17;
        constexpr i16 F18          = VK_F18;
        constexpr i16 F19          = VK_F19;
        constexpr i16 F20          = VK_F20;
        constexpr i16 F21          = VK_F21;
        constexpr i16 F22          = VK_F22;
        constexpr i16 F23          = VK_F23;
        constexpr i16 F24          = VK_F24;
        constexpr i16 F25          = VK_F24 + 1;  // Win32 has no VK_F24-past key; kept distinct, never sent.
        constexpr i16 Kp0          = VK_NUMPAD0;
        constexpr i16 Kp1          = VK_NUMPAD1;
        constexpr i16 Kp2          = VK_NUMPAD2;
        constexpr i16 Kp3          = VK_NUMPAD3;
        constexpr i16 Kp4          = VK_NUMPAD4;
        constexpr i16 Kp5          = VK_NUMPAD5;
        constexpr i16 Kp6          = VK_NUMPAD6;
        constexpr i16 Kp7          = VK_NUMPAD7;
        constexpr i16 Kp8          = VK_NUMPAD8;
        constexpr i16 Kp9          = VK_NUMPAD9;
        constexpr i16 KpDecimal    = VK_DECIMAL;
        constexpr i16 KpDivide     = VK_DIVIDE;
        constexpr i16 KpMultiply   = VK_MULTIPLY;
        constexpr i16 KpSubtract   = VK_SUBTRACT;
        constexpr i16 KpAdd        = VK_ADD;
        constexpr i16 KpEnter      = VK_RETURN;  // Win32 reports the same VK for main and numpad Enter.
        constexpr i16 KpEqual      = VK_OEM_NEC_EQUAL;
        constexpr i16 LeftShift    = VK_LSHIFT;
        constexpr i16 LeftControl  = VK_LCONTROL;
        constexpr i16 LeftAlt      = VK_LMENU;
        constexpr i16 LeftSuper    = VK_LWIN;
        constexpr i16 RightShift   = VK_RSHIFT;
        constexpr i16 RightControl = VK_RCONTROL;
        constexpr i16 RightAlt     = VK_RMENU;
        constexpr i16 RightSuper   = VK_RWIN;
        constexpr i16 Menu         = VK_APPS;
        constexpr i16 Last         = Menu;

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

    // Win32 has no single "mouse button code" table the way GLFW does - only
    // VK_LBUTTON/VK_RBUTTON/VK_MBUTTON/VK_XBUTTON1/VK_XBUTTON2 exist as real
    // virtual-key codes. Button6-8 have no Win32 equivalent at all (WM_XBUTTON*
    // only carries XBUTTON1/XBUTTON2 in its wParam), so they're just distinct
    // placeholders the Win32 window will never actually send - kept for
    // interface parity with GLFW's 8-button model. Mouse and keyboard states
    // live in separate maps in InputManager, so these values only need to be
    // unique from each other, not from Input::KeyCode's.
    namespace Input::MouseButton {
        constexpr i16 Left    = VK_LBUTTON;
        constexpr i16 Right   = VK_RBUTTON;
        constexpr i16 Middle  = VK_MBUTTON;
        constexpr i16 Button1 = Left;
        constexpr i16 Button2 = Right;
        constexpr i16 Button3 = Middle;
        constexpr i16 Button4 = VK_XBUTTON1;
        constexpr i16 Button5 = VK_XBUTTON2;
        constexpr i16 Button6 = VK_XBUTTON2 + 1;
        constexpr i16 Button7 = VK_XBUTTON2 + 2;
        constexpr i16 Button8 = VK_XBUTTON2 + 3;
        constexpr i16 Last    = Button8;

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

        // VK_* codes (and this engine's MouseButton placeholders) are all single
        // bytes, so a flat array indexed directly by code is both correct and
        // faster than a hash map - and unlike map[key], querying a key that was
        // never pressed can't silently grow anything.
        static constexpr size_t STATE_TABLE_SIZE = 256;
        using StateTable                         = std::array<bool, STATE_TABLE_SIZE>;

    public:
        // --- Level-triggered: true for as long as the key/button is held. ---
        NODISCARD bool GetKeyDown(const i16 Key) const { return IsSet(_KeyStates, Key); }
        NODISCARD bool GetKeyUp(const i16 Key) const { return !IsSet(_KeyStates, Key); }
        NODISCARD bool GetMouseButtonDown(const i16 Button) const { return IsSet(_MouseButtonStates, Button); }
        NODISCARD bool GetMouseButtonUp(const i16 Button) const { return !IsSet(_MouseButtonStates, Button); }

        // --- Edge-triggered: true for exactly the one frame the state changed. ---
        NODISCARD bool WasKeyPressed(const i16 Key) const {
            return IsSet(_KeyStates, Key) && !IsSet(_PrevKeyStates, Key);
        }
        NODISCARD bool WasKeyReleased(const i16 Key) const {
            return !IsSet(_KeyStates, Key) && IsSet(_PrevKeyStates, Key);
        }
        NODISCARD bool WasMouseButtonPressed(const i16 Button) const {
            return IsSet(_MouseButtonStates, Button) && !IsSet(_PrevMouseButtonStates, Button);
        }
        NODISCARD bool WasMouseButtonReleased(const i16 Button) const {
            return !IsSet(_MouseButtonStates, Button) && IsSet(_PrevMouseButtonStates, Button);
        }

        NODISCARD bool GetAction(const std::string& Name) const {
            if (!_InputMap.IsLoaded()) return false;

            const auto It = _InputMap.GetActions().find(Name);
            if (It == _InputMap.GetActions().end()) return false;

            const auto& [KeyCodes, MouseButtons] = It->second;
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

        static bool IsSet(const StateTable& States, const i16 Code) {
            return Code >= 0 && CAST<size_t>(Code) < STATE_TABLE_SIZE && States[Code];
        }

        InputMap _InputMap;
        StateTable _KeyStates {};
        StateTable _PrevKeyStates {};
        StateTable _MouseButtonStates {};
        StateTable _PrevMouseButtonStates {};
        i32 _MouseX {0}, _MouseY {0};
        i32 _MouseDeltaX {0}, _MouseDeltaY {0};
        bool _Enabled {true};

        void LoadInputMap(const std::filesystem::path& InputConfig) { _InputMap.Load(InputConfig); }

        void SetEnabled(const bool Enabled) { _Enabled = Enabled; }

        void UpdateKeyState(const i16 KeyCode, const bool Pressed) {
            if (!_Enabled || KeyCode < 0 || CAST<size_t>(KeyCode) >= STATE_TABLE_SIZE) return;
            _KeyStates[KeyCode] = Pressed;
        }

        void UpdateMouseButtonState(const i16 Button, const bool Pressed) {
            if (!_Enabled || Button < 0 || CAST<size_t>(Button) >= STATE_TABLE_SIZE) return;
            _MouseButtonStates[Button] = Pressed;
        }

        /// @brief Absolute client-area cursor position, from WM_MOUSEMOVE.
        void SetMousePosition(const i32 X, const i32 Y) {
            if (!_Enabled) return;
            _MouseX = X;
            _MouseY = Y;
        }

        /// @brief Accumulates one raw-input mouse-motion event. Summed rather
        /// than overwritten: a high-poll-rate mouse can report several WM_INPUT
        /// motion events between two frames, and only summing captures all of it.
        void AddMouseDelta(const i32 DeltaX, const i32 DeltaY) {
            if (!_Enabled) return;
            _MouseDeltaX += DeltaX;
            _MouseDeltaY += DeltaY;
        }

        /// @brief Called once per frame (Window::ResetInput, driven by
        /// Game::TickFrame) after gameplay code has had the chance to read this
        /// frame's state - snapshots it as "previous" for edge detection and
        /// clears the per-frame mouse delta accumulator.
        void EndFrame() {
            _PrevKeyStates         = _KeyStates;
            _PrevMouseButtonStates = _MouseButtonStates;
            _MouseDeltaX           = 0;
            _MouseDeltaY           = 0;
        }
    };
}  // namespace Xen