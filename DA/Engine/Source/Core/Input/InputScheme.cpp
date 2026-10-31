#include "PCH.h"
#include "InputScheme.h"
#include "Input.h"

#include <algorithm>

namespace DA {
    // clang-format off
    bool KeyboardInputButton::IsDown(const Input& input) const { return input.IsKeyDown(m_key); }
    bool MouseInputButton::IsDown(const Input& input) const { return input.IsMouseDown(m_button); }
    // clang-format on

    bool CompoundInputButton::IsDown(const Input& input) const {
        if (m_buttons.empty()) return false;
        if (m_useAnd) {
            return std::ranges::all_of(m_buttons, [&](const auto& b) { return b->IsDown(input); });
        }
        return std::ranges::any_of(m_buttons, [&](const auto& b) { return b->IsDown(input); });
    }

    bool AxisInputButton::IsDown(const Input& input) const {
        const float v = m_axis->GetAmount(input);
        return m_min < v && v < m_max;
    }

    float ButtonInputAxis::GetAmount(const Input& input) const {
        float amount = 0.0f;
        if (m_positive->IsDown(input)) amount += 1.0f;
        if (m_negative->IsDown(input)) amount -= 1.0f;
        return Scale * amount + Offset;
    }

    float MouseInputAxis::GetAmount(const Input& input) const {
        const glm::vec2 d = input.GetMouseDelta();
        const float v = (m_axis == MouseAxisId::X) ? d.x : d.y;
        return Scale * v + Offset;
    }

    float CompoundInputAxis::GetAmount(const Input& input) const {
        float sum = 0.0f;
        for (const auto& a : m_axes)
            sum += a->GetAmount(input);
        if (m_clamp) sum = std::clamp(sum, -1.0f, 1.0f);
        return Scale * sum + Offset;
    }

    namespace Bind {
        std::unique_ptr<InputButton> KeyButton(Key key) {
            return std::make_unique<KeyboardInputButton>(key);
        }
        std::unique_ptr<InputButton> MouseButtonSource(MouseButton b) {
            return std::make_unique<MouseInputButton>(b);
        }
        std::unique_ptr<InputButton> AnyOf(std::vector<std::unique_ptr<InputButton>> buttons) {
            return std::make_unique<CompoundInputButton>(std::move(buttons), false);
        }
        std::unique_ptr<InputAxis> KeyAxis(Key negative, Key positive) {
            return std::make_unique<ButtonInputAxis>(KeyButton(negative), KeyButton(positive));
        }
        std::unique_ptr<InputAxis> MouseDelta(MouseAxisId axis, float scale) {
            return std::make_unique<MouseInputAxis>(axis, scale);
        }
    } // namespace Bind

    InputAxis* InputScheme::AddAxis(const std::string& name, std::unique_ptr<InputAxis> axis) {
        InputAxis* raw = axis.get();
        m_axes[name] = std::move(axis);
        return raw;
    }

    InputButton* InputScheme::AddButton(const std::string& name, std::unique_ptr<InputButton> button) {
        InputButton* raw = button.get();
        m_buttons[name] = std::move(button);
        return raw;
    }

    InputAxis* InputScheme::GetAxis(const std::string& name) const {
        const auto it = m_axes.find(name);
        return it == m_axes.end() ? nullptr : it->second.get();
    }

    InputButton* InputScheme::GetButton(const std::string& name) const {
        const auto it = m_buttons.find(name);
        return it == m_buttons.end() ? nullptr : it->second.get();
    }

    void InputScheme::Update(const Input& input, bool sync) {
        for (auto& [name, button] : m_buttons)
            button->Update(input, sync);
    }
} // namespace DA
