#include "PCH.h"
#include "Input.h"
#include <Platform/Window.h>

namespace DA {
    namespace {
        // GLFW's valid printable/function key range.
        constexpr int FirstKey = 32;
        constexpr int LastKey = 32;
    } // namespace

    Input::Input(Window& window) : m_window(window) {}

    void Input::Update() {
        m_prevKeys = m_keys;
        m_prevMouse = m_mouse;

        for (int k = FirstKey; k <= LastKey; ++k)
            m_keys[static_cast<size_t>(k)] = m_window.IsKeyDown(k);
        for (size_t b = 0; b < MouseButtonCount; ++b)
            m_mouse[b] = m_window.IsMouseButtonDown(static_cast<int>(b));

        double x = 0.0;
        double y = 0.0;
        m_window.GetCursorPosition(x, y);
        if (m_hasLastPos && !m_skipDelta) {
            m_mouseDelta = {static_cast<float>(x - m_lastX), static_cast<float>(y - m_lastY)};
        } else {
            m_mouseDelta = {0.0f, 0.0f};
        }
        m_lastX = x;
        m_lastY = y;
        m_hasLastPos = true;
        m_skipDelta = false;
    }

    void Input::SetCursorCaptured(bool captured) {
        if (captured == m_captured) return;
        m_window.SetCursorCaptured(captured);
        m_captured = captured;
        m_skipDelta = true; // the cursor position jumps when the mode changes
    }

    void Input::BindAction(const std::string& name, std::vector<Key> keys) {
        m_actions[name] = std::move(keys);
    }

    void Input::BindAxis(const std::string& name, std::vector<AxisBinding> bindings) {
        m_axes[name] = std::move(bindings);
    }

    bool Input::IsActionDown(const std::string& name) const {
        const auto it = m_actions.find(name);
        if (it == m_actions.end()) return false;
        return std::ranges::any_of(it->second, [&](Key k) { return IsKeyDown(k); });
    }

    bool Input::IsActionPressed(const std::string& name) const {
        const auto it = m_actions.find(name);
        if (it == m_actions.end()) return false;
        return std::ranges::any_of(it->second, [&](Key k) { return IsKeyPressed(k); });
    }

    bool Input::IsActionReleased(const std::string& name) const {
        const auto it = m_actions.find(name);
        if (it == m_actions.end()) return false;
        return std::ranges::any_of(it->second, [&](Key k) { return IsKeyReleased(k); });
    }

    float Input::GetAxis(const std::string& name) const {
        const auto it = m_axes.find(name);
        if (it == m_axes.end()) return 0.0f;

        float keys = 0.0f;
        float mouse = 0.0f;
        for (const AxisBinding& b : it->second) {
            switch (b.Type) {
            case AxisBinding::Source::Key:
                if (IsKeyDown(b.KeyCode)) keys += b.Scale;
                break;
            case AxisBinding::Source::MouseX:
                mouse += m_mouseDelta.x * b.Scale;
                break;
            case AxisBinding::Source::MouseY:
                mouse += m_mouseDelta.y * b.Scale;
                break;
            }
        }
        return std::clamp(keys, -1.0f, 1.0f) + mouse;
    }
} // namespace DA
