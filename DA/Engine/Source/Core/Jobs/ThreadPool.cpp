#include "PCH.h"
#include "ThreadPool.h"
#include <Core/Log.h>

namespace DA {
    ThreadPool::ThreadPool(unsigned threadCount) {
        if (threadCount == 0) {
            const unsigned hw = std::thread::hardware_concurrency();
            threadCount = hw > 2 ? hw - 2 : 1;
        }
        for (unsigned i = 0; i < threadCount; ++i) {
            m_workers.emplace_back([this](std::stop_token stop) { WorkerLoop(stop); });
        }
    }

    ThreadPool::~ThreadPool() {
        for (auto& w : m_workers)
            w.request_stop();
        m_workers.clear();
    }

    void ThreadPool::Enqueue(std::function<void()> job) {
        {
            std::lock_guard lock(m_mutex);
            m_queue.push_back(std::move(job));
        }
        m_cv.notify_one();
    }

    void ThreadPool::WorkerLoop(std::stop_token stop) {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock lock(m_mutex);
                m_cv.wait(lock, stop, [this] { return !m_queue.empty(); });
                if (stop.stop_requested()) return;
                job = std::move(m_queue.front());
                m_queue.pop_front();
            }
            try {
                job();
            } catch (const std::exception& e) {
                DA_ERROR("Job threw: {}", e.what());
            } catch (...) {
                DA_ERROR("Job threw an unknown exception.");
            }
        }
    }
} // namespace DA
