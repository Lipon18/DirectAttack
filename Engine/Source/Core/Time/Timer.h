#pragma once

#include "Core/DA_API.h"

#include <cstdint>
#include <functional>
#include <optional>

namespace DA {
    class Timers;

    class DA_API Timer {
        friend class Timers;

    public:
        double GetInterval() const { return m_interval; }
        bool IsDestroyed() const { return m_destroyed; }

        // Cancels this timer. Removal happens during the manager's update.
        void Destroy() { m_destroyed = true; }

    private:
        Timer(double interval, std::optional<uint32_t> repeat, std::function<void()> callback);

        double m_interval;
        double m_remaining;
        std::optional<uint32_t> m_repeat;
        std::function<void()> m_callback;
        bool m_destroyed = false;
    };
} // namespace DA
