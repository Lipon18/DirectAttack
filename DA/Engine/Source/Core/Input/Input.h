#pragma once

#include <Core/DA_API.h>
#include "InputCodes.h"
#include "InputScheme.h"

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>

#include <glm/vec2.hpp>

namespace DA {
    class Window;

    class DA_API Input {
    public:
        explicit Input(Window& window);

        // Call once per frame, right after the window polled its events.
        void Update();

        // Raw state (what the binding sources read)
        bool IsKeyDown(Key key) const { return m_keys[static_cast<size_t>(key)]; }
        bool IsKeyPressed(Key key) const {
            return m_keys[static_cast<size_t>(key)] && !m_prevKeys[static_cast<size_t>(key)];
        }
        bool IsKeyReleased(Key key) const {
            return !m_keys[static_cast<size_t>(key)] && m_prevKeys[static_cast<size_t>(key)];
        }
        bool IsMouseDown(MouseButton b) const { return m_mouse[static_cast<size_t>(b)]; }
        bool IsMousePressed(MouseButton b) const {
            return m_mouse[static_cast<size_t>(b)] && !m_prevMouse[static_cast<size_t>(b)];
        }
        glm::vec2 GetMouseDelta() const { return m_mouseDelta; }

        void SetCursorCaptured(bool captured);
        bool IsCursorCaptured() const { return m_captured; }

        // Schemes. A "Default" scheme exists and is current from the start.
        InputScheme& GetScheme() { return *m_current; }
        InputScheme* GetScheme(const std::string& name) const;
        InputScheme& AddScheme(const std::string& name, bool makeCurrent = false);
        bool SetScheme(const std::string& name);

        // Named queries against the current scheme. Unknown names return 0 / false.
        float GetAxis(const std::string& name) const;
        bool IsButtonDown(const std::string& name) const;
        bool IsButtonPressed(const std::string& name) const;
        bool IsButtonReleased(const std::string& name) const;

    private:
        static constexpr size_t KeyCount = 512;
        static constexpr size_t MouseButtonCount = 8;

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

        std::unordered_map<std::string, std::unique_ptr<InputScheme>> m_schemes;
        InputScheme* m_current = nullptr;
    };
} // namespace DA
