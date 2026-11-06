#pragma once

#include <deque>
#include <functional>
#include <mutex>

namespace DA {
    // Workers post results here; the main thread drains it once per frame.
    class MainThreadQueue {
    public:
        void Post(std::function<void()> fn) {
            std::lock_guard lock(m_mutex);
            m_queue.push_back(std::move(fn));
        }

        size_t Drain() {
            std::deque<std::function<void()>> local;
            {
                std::lock_guard lock(m_mutex);
                local.swap(m_queue);
            }
            for (auto& fn : local)
                fn();
            return local.size();
        }

    private:
        std::mutex m_mutex;
        std::deque<std::function<void()>> m_queue;
    };
} // namespace DA
