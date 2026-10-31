#pragma once

#include <Core/DA_API.h>
#include "InputCodes.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace DA {
    class Input;

    // Binary source. Subclass for new devices (gamepad button, ...).
    class DA_API InputButton {
    public:
        virtual ~InputButton() = default;
        virtual bool IsDown(const Input& input) const = 0;

        bool Down() const { return m_down; }
        bool Pressed() const { return m_down && !m_prevDown; }
        bool Released() const { return !m_down && m_prevDown; }

        // Called once per frame by the owning scheme. `sync` = make "previous" equal "current" (no false edges).
        void Update(const Input& input, bool sync = false) {
            m_prevDown = sync ? IsDown(input) : m_down;
            m_down = IsDown(input);
        }

    private:
        bool m_down = false;
        bool m_prevDown = false;
    };

    // Analog source. GetAmount is evaluated on demand.
    class DA_API InputAxis {
    public:
        virtual ~InputAxis() = default;
        virtual float GetAmount(const Input& input) const = 0;

        float Scale = 1.0f;
        float Offset = 0.0f;
    };

    class DA_API KeyboardInputButton final : public InputButton {
    public:
        explicit KeyboardInputButton(Key key) : m_key(key) {}
        bool IsDown(const Input& input) const override;

    private:
        Key m_key;
    };

    class DA_API MouseInputButton final : public InputButton {
    public:
        explicit MouseInputButton(MouseButton button) : m_button(button) {}
        bool IsDown(const Input& input) const override;

    private:
        MouseButton m_button;
    };

    // Down when any (or all, with useAnd) child buttons are down.
    class DA_API CompoundInputButton final : public InputButton {
    public:
        explicit CompoundInputButton(std::vector<std::unique_ptr<InputButton>> buttons, bool useAnd = false)
            : m_buttons(std::move(buttons)), m_useAnd(useAnd) {}
        bool IsDown(const Input& input) const override;

    private:
        std::vector<std::unique_ptr<InputButton>> m_buttons;
        bool m_useAnd;
    };

    // Down while another axis is strictly between min and max.
    class DA_API AxisInputButton final : public InputButton {
    public:
        AxisInputButton(std::unique_ptr<InputAxis> axis, float min, float max)
            : m_axis(std::move(axis)), m_min(min), m_max(max) {}
        bool IsDown(const Input& input) const override;

    private:
        std::unique_ptr<InputAxis> m_axis;
        float m_min;
        float m_max;
    };

    // -1 while `negative` is down, +1 while `positive` is down.
    class DA_API ButtonInputAxis final : public InputAxis {
    public:
        ButtonInputAxis(std::unique_ptr<InputButton> negative, std::unique_ptr<InputButton> positive)
            : m_negative(std::move(negative)), m_positive(std::move(positive)) {}
        float GetAmount(const Input& input) const override;

    private:
        std::unique_ptr<InputButton> m_negative;
        std::unique_ptr<InputButton> m_positive;
    };

    // Mouse movement in pixels since the previous frame (not scaled by delta time).
    class DA_API MouseInputAxis final : public InputAxis {
    public:
        explicit MouseInputAxis(MouseAxisId axis, float scale = 1.0f) : m_axis(axis) { Scale = scale; }
        float GetAmount(const Input& input) const override;

    private:
        MouseAxisId m_axis;
    };

    // Sum of child axes. Clamped to [-1, 1] by default, which suits key-based axes. Turn it off for mouse axes.
    class DA_API CompoundInputAxis final : public InputAxis {
    public:
        explicit CompoundInputAxis(std::vector<std::unique_ptr<InputAxis>> axes, bool clamp = true)
            : m_axes(std::move(axes)), m_clamp(clamp) {}
        float GetAmount(const Input& input) const override;

    private:
        std::vector<std::unique_ptr<InputAxis>> m_axes;
        bool m_clamp;
    };

    // Convenience factories so bindings read on one line.
    namespace Bind {
        DA_API std::unique_ptr<InputButton> KeyButton(Key key);
        DA_API std::unique_ptr<InputButton> MouseButtonSource(MouseButton button);
        DA_API std::unique_ptr<InputButton> AnyOf(std::vector<std::unique_ptr<InputButton>> buttons);
        DA_API std::unique_ptr<InputAxis> KeyAxis(Key negative, Key positive);
        DA_API std::unique_ptr<InputAxis> MouseDelta(MouseAxisId axis, float scale = 1.0f);
    } // namespace Bind

    // A named set of axes and buttons. Swap schemes to change control contexts (gameplay, menu, ...).
    class DA_API InputScheme {
    public:
        InputAxis* AddAxis(const std::string& name, std::unique_ptr<InputAxis> axis);
        InputButton* AddButton(const std::string& name, std::unique_ptr<InputButton> button);
        void RemoveAxis(const std::string& name) { m_axes.erase(name); }
        void RemoveButton(const std::string& name) { m_buttons.erase(name); }

        // Null if the name is unknown.
        InputAxis* GetAxis(const std::string& name) const;
        InputButton* GetButton(const std::string& name) const;

        void Update(const Input& input, bool sync = false);

    private:
        std::unordered_map<std::string, std::unique_ptr<InputAxis>> m_axes;
        std::unordered_map<std::string, std::unique_ptr<InputButton>> m_buttons;
    };
} // namespace DA
