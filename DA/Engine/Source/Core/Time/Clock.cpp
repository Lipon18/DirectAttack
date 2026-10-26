#include "PCH.h"
#include "Clock.h"

namespace DA {
    Clock::Clock() {
        Reset();
    }

    void Clock::Update() {
        const auto now = NativeClock::now();
        m_deltaSeconds = std::chrono::duration<float>(now - m_last).count();
        m_last = now;
    }

    void Clock::Reset() {
        m_start = NativeClock::now();
        m_last = m_start;
        m_deltaSeconds = 0.0f;
    }

    double Clock::GetElapsedSeconds() const {
        return std::chrono::duration<double>(NativeClock::now() - m_start).count();
    }
} // namespace DA
