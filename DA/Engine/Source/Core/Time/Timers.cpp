#include "PCH.h"
#include "Timers.h"

namespace DA {
    Timer* Timers::Add(std::function<void()> callback, double intervalSeconds, std::optional<uint32_t> repeat) {
        if (!callback || !std::isfinite(intervalSeconds) || intervalSeconds <= 0.0) {
            return nullptr;
        }

        auto timer = std::unique_ptr<Timer>(new Timer(intervalSeconds, repeat, std::move(callback)));

        Timer* result = timer.get();

        // Do not modify the active collection while iterating it.
        if (m_updating) {
            m_pendingTimers.push_back(std::move(timer));
        } else {
            m_timers.push_back(std::move(timer));
        }

        return result;
    }

    Timer* Timers::Once(std::function<void()> callback, double delaySeconds) {
        return Add(std::move(callback), delaySeconds, 1);
    }

    Timer* Timers::Every(std::function<void()> callback, double intervalSeconds) {
        return Add(std::move(callback), intervalSeconds, std::nullopt);
    }

    Timer* Timers::Repeat(std::function<void()> callback, double intervalSeconds, uint32_t repeat) {
        if (repeat == 0) {
            return nullptr;
        }

        return Add(std::move(callback), intervalSeconds, repeat);
    }

    void Timers::Update(float deltaSeconds) {
        if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f) {
            return;
        }

        m_updating = true;

        for (auto& timer : m_timers) {
            if (timer->m_destroyed) {
                continue;
            }

            timer->m_remaining -= deltaSeconds;

            if (timer->m_remaining > 0.0) {
                continue;
            }

            // One callback per timer per frame.
            timer->m_remaining = timer->m_interval;

            // Decrement before invocation so the callback can safely
            // inspect or destroy its own timer.
            if (timer->m_repeat && --(*timer->m_repeat) == 0) {
                timer->m_destroyed = true;
            }

            timer->m_callback();
        }

        m_updating = false;

        std::erase_if(m_timers, [](const auto& timer) { return timer->m_destroyed; });

        // Timers scheduled inside callbacks become active next frame.
        for (auto& timer : m_pendingTimers) {
            m_timers.push_back(std::move(timer));
        }

        m_pendingTimers.clear();
    }

    void Timers::Clear() {
        if (m_updating) {
            for (auto& timer : m_timers) {
                timer->m_destroyed = true;
            }

            for (auto& timer : m_pendingTimers) {
                timer->m_destroyed = true;
            }

            return;
        }

        m_timers.clear();
        m_pendingTimers.clear();
    }
} // namespace DA
