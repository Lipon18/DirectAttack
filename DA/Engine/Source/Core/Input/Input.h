#pragma once

#include <Core/DA_API.h>

#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>

namespace DA {
    class Window;

    // Numeric values match GLFW key codes, so the platform layer needs no translation table.
    enum class Key : int {
        Space = 32,
        A = 65,
        D = 68,
        E = 69,
        F = 70,
        Q = 81,
        R = 82,
        S = 83,
        W = 87,
        Escape = 256,
        Enter = 257,
        Tab = 258,
        LeftShift = 340,
        LeftControl = 341,
    };

    enum class MouseButton : int {
        Left = 0,
        Right = 1,
        Middle = 2
    };

    // One contribution to a named axis: a key worth +/- scale, or the mouse delta times scale.
    struct AxisBinding {
        enum class Source {
            Key,
            MouseX,
            MouseY
        };

        Source Type = Source::Key;
        Key KeyCode = Key::Space;
        float Scale = 1.0f;

        static AxisBinding FromKey(Key key, float scale) { return {Source::Key, key, scale}; }
        static AxisBinding MouseX(float scale = 1.0f) { return {Source::MouseX, Key::Space, scale}; }
        static AxisBinding MouseY(float scale = 1.0f) { return {Source::MouseY, Key::Space, scale}; }
    };

    class DA_API Input {
    public:
        explicit Input(Window& window);

        // Call once per frame, right after the window polled its events.
        void Update();

        // Raw state
        bool IsKeyDown(Key key) const { return m_keys[Index(key)]; }
        bool IsKeyPressed(Key key) const { return m_keys[Index(key)] && !m_prevKeys[Index(key)]; }
        bool IsKeyReleased(Key key) const { return !m_keys[Index(key)] && m_prevKeys[Index(key)]; }
        bool IsMouseDown(MouseButton b) const { return m_mouse[static_cast<size_t>(b)]; }
        bool IsMousePressed(MouseButton b) const {
            return m_mouse[static_cast<size_t>(b)] && !m_prevMouse[static_cast<size_t>(b)];
        }
        glm::vec2 GetMouseDelta() const { return m_mouseDelta; } // pixels moved since the previous Update

        // The cursor stays visible and free until captured. Capturing skips one frame of mouse delta (no jump).
        void SetCursorCaptured(bool captured);
        bool IsCursorCaptured() const { return m_captured; }

        // Gameplay layer: bind once, query by name.
        void BindAction(const std::string& name, std::vector<Key> keys);
        void BindAxis(const std::string& name, std::vector<AxisBinding> bindings);

        bool IsActionDown(const std::string& name) const;
        bool IsActionPressed(const std::string& name) const;
        bool IsActionReleased(const std::string& name) const;
        // Key sources are summed and clamped to [-1, 1]; mouse sources are added unclamped.
        float GetAxis(const std::string& name) const;

    private:
        static constexpr size_t KeyCount = 512;
        static constexpr size_t MouseButtonCount = 8;
        static size_t Index(Key key) { return static_cast<size_t>(key); }

        Window& m_window;

        std::array<bool, KeyCount> m_keys{};
        std::array<bool, KeyCount> m_prevKeys{};
        std::array<bool, MouseButtonCount> m_mouse{};
        std::array<bool, MouseButtonCount> m_prevMouse{};

        glm::vec2 m_mouseDelta{0.0f};
        double m_lastX = 0.0;
        double m_lastY = 0.0;
        bool m_hasLastPos = false;
        bool m_skipDelta = false;
        bool m_captured = false;

        std::unordered_map<std::string, std::vector<Key>> m_actions;
        std::unordered_map<std::string, std::vector<AxisBinding>> m_axes;
    };
} // namespace DA
