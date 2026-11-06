#pragma once

#include "Core/DA_API.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace DA {
    // Fixed worker pool. Jobs must not touch Vulkan or other main-thread state.
    class DA_API ThreadPool {
    public:
        explicit ThreadPool(unsigned threadCount = 0); // 0 = hardware threads - 2 (at least 1)
        ~ThreadPool();                                 // drops queued jobs, waits for running ones

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        void Enqueue(std::function<void()> job);
        unsigned GetThreadCount() const { return static_cast<unsigned>(m_workers.size()); }

    private:
        void WorkerLoop(std::stop_token stop);

        std::mutex m_mutex;
        std::condition_variable_any m_cv;
        std::deque<std::function<void()>> m_queue;
        std::vector<std::jthread> m_workers; // last member: destroyed (joined) first
    };
} // namespace DA
