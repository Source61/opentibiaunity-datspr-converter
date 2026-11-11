#pragma once

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace otu {

// Fixed pool of worker threads. push() blocks while the backlog is full, so a producer
// that hands over large buffers (raw sprite sheets) cannot run far ahead of the workers.
class WorkQueue {
public:
    explicit WorkQueue(unsigned threads = 0) {
        if (threads == 0)
            threads = std::max(1u, std::thread::hardware_concurrency());
        m_maxPending = threads * 2;
        for (unsigned i = 0; i < threads; i++)
            m_workers.emplace_back([this] { run(); });
    }

    ~WorkQueue() {
        {
            std::lock_guard lock(m_mutex);
            m_stopping = true;
        }
        m_workAvailable.notify_all();
        for (auto& worker : m_workers)
            worker.join();
    }

    WorkQueue(const WorkQueue&) = delete;
    WorkQueue& operator=(const WorkQueue&) = delete;

    void push(std::function<void()> job) {
        std::unique_lock lock(m_mutex);
        m_spaceAvailable.wait(lock, [this] { return m_jobs.size() < m_maxPending; });
        m_jobs.push_back(std::move(job));
        m_workAvailable.notify_one();
    }

    // Blocks until all queued jobs have finished, then rethrows the first exception any of
    // them threw.
    void wait() {
        std::unique_lock lock(m_mutex);
        m_idle.wait(lock, [this] { return m_jobs.empty() && m_running == 0; });
        if (m_error)
            std::rethrow_exception(std::exchange(m_error, nullptr));
    }

private:
    void run() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock lock(m_mutex);
                m_workAvailable.wait(lock, [this] { return m_stopping || !m_jobs.empty(); });
                if (m_jobs.empty())
                    return;
                job = std::move(m_jobs.front());
                m_jobs.pop_front();
                m_running++;
            }
            m_spaceAvailable.notify_one();

            std::exception_ptr error;
            try {
                job();
            } catch (...) {
                error = std::current_exception();
            }

            {
                std::lock_guard lock(m_mutex);
                m_running--;
                if (error && !m_error)
                    m_error = error;
            }
            m_idle.notify_all();
        }
    }

    std::mutex m_mutex;
    std::condition_variable m_workAvailable, m_spaceAvailable, m_idle;
    std::deque<std::function<void()>> m_jobs;
    std::vector<std::thread> m_workers;
    size_t m_maxPending = 0;
    unsigned m_running = 0;
    bool m_stopping = false;
    std::exception_ptr m_error;
};

} // namespace otu
