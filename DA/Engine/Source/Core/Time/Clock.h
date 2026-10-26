#pragma once

#include <Core/DA_API.h>

#include <chrono>

namespace DA {
    class DA_API Clock {
    public:
        using NativeClock = std::chrono::steady_clock;

        Clock();

        // Updates the clock. Call once per frame
        void Update();

        // Resets elapsed time and frame delta.
        void Reset();

        // Time elapsed during the previous update, in seconds.
        float DeltaSeconds() const { return m_deltaSeconds; }

        // Total time since construction or the last reset.
        double GetElapsedSeconds() const;

        // Time since the previous update, in milliseconds.
        double GetDeltaMilliseconds() const { return static_cast<double>(m_deltaSeconds) * 1000.0; }

    private:
        NativeClock::time_point m_start;
        NativeClock::time_point m_last;
        float m_deltaSeconds = 0.0f;
    };
} // namespace DA
