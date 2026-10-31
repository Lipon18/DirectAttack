#include "PCH.h"
#include "Input.h"
#include <Core/Log.h>
#include <Platform/Window.h>

namespace DA {
    namespace {
        constexpr int FirstKey = 32; // GLFW's valid key range
        constexpr int LastKey = 348;
    } // namespace

    Input::Input(Window& window) : m_window(window) {
        AddScheme("Default", true);
    }

    void Input::Update() {
        m_prevKeys = m_keys;
        m_prevMouse = m_mouse;

        // clang-format off
        for (int k = FirstKey; k <= LastKey; ++k) m_keys[static_cast<size_t>(k)] = m_window.IsKeyDown(k);
        for (size_t b = 0; b < MouseButtonCount; ++b) m_mouse[b] = m_window.IsMouseButtonDown(static_cast<int>(b));
        // clang-format on

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

        m_current->Update(*this);
    }

    void Input::SetCursorCaptured(bool captured) {
        if (captured == m_captured) return;
        m_window.SetCursorCaptured(captured);
        m_captured = captured;
        m_skipDelta = true; // the cursor position jumps when the mode changes
    }

    InputScheme* Input::GetScheme(const std::string& name) const {
        const auto it = m_schemes.find(name);
        return it == m_schemes.end() ? nullptr : it->second.get();
    }

    InputScheme& Input::AddScheme(const std::string& name, bool makeCurrent) {
        auto& slot = m_schemes[name];
        if (!slot) slot = std::make_unique<InputScheme>();
        if (makeCurrent || !m_current) {
            m_current = slot.get();
            m_current->Update(*this, /*sync*/ true);
        }
        return *slot;
    }

    bool Input::SetScheme(const std::string& name) {
        InputScheme* scheme = GetScheme(name);
        if (!scheme) {
            DA_WARN("Input scheme not found: {}", name);
            return false;
        }
        m_current = scheme;
        m_current->Update(*this, /*sync*/ true); // held keys must not fire a fake press
        return true;
    }

    float Input::GetAxis(const std::string& name) const {
        const InputAxis* axis = m_current->GetAxis(name);
        return axis ? axis->GetAmount(*this) : 0.0f;
    }

    bool Input::IsButtonDown(const std::string& name) const {
        const InputButton* b = m_current->GetButton(name);
        return b && b->Down();
    }

    bool Input::IsButtonPressed(const std::string& name) const {
        const InputButton* b = m_current->GetButton(name);
        return b && b->Pressed();
    }

    bool Input::IsButtonReleased(const std::string& name) const {
        const InputButton* b = m_current->GetButton(name);
        return b && b->Released();
    }
} // namespace DA
