#include "PCH.h"
#include "Core/Time/Timer.h"

#include <utility>

namespace DA {
    Timer::Timer(double interval, std::optional<uint32_t> repeat, std::function<void()> callback)
        : m_interval(interval), m_remaining(interval), m_repeat(repeat), m_callback(std::move(callback)) {}

} // namespace DA
