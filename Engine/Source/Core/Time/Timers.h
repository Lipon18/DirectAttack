#pragma once

#include <Core/DA_API.h>
#include "Timer.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace DA {

    class DA_API Timers {
    public:
        Timers() = default;
        ~Timers() = default;

        Timers(const Timers&) = delete;
        Timers& operator=(const Timers&) = delete;

        // Executes once after delaySeconds.
        Timer* Once(std::function<void()> callback, double delaySeconds);

        // Executes indefinitely at intervalSeconds.
        Timer* Every(std::function<void()> callback, double intervalSeconds);

        // Executes at most repeat times.
        Timer* Repeat(std::function<void()> callback, double intervalSeconds, uint32_t repeat);

        // Call once per frame on the main thread.
        void Update(float deltaSeconds);

        // Cancels all active timers.
        void Clear();

    private:
        Timer* Add(std::function<void()> callback, double intervalSeconds, std::optional<uint32_t> repeat);

        std::vector<std::unique_ptr<Timer>> m_timers;
        std::vector<std::unique_ptr<Timer>> m_pendingTimers;

        bool m_updating = false;
    };
} // namespace DA
