#pragma once
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <functional>
#include <atomic>


namespace tp
{

    struct TaskQueue
    {
        std::queue<std::function<void()>> tasks;
        std::mutex mutex;
        std::atomic<uint32_t> remaining_tasks = 0;
        bool pause = false;

        template<typename Callback_Function>
        void add_work_queue(Callback_Function&& callback)
        {
            std::lock_guard<std::mutex> lock_guard{mutex};
            tasks = std::forward<Callback_Function>(callback);
            remaining_tasks = callback.size();
        }

        void get_task(std::function<void()>& target_callback)
        {
            std::lock_guard<std::mutex> lock_guard{mutex};
            if (tasks.empty() | pause) 
            {
                return;
            }
            target_callback = std::move(tasks.front());
            tasks.pop();
        }

        static void wait()
        {
            std::this_thread::yield();
        }

        void wait_for_completion() const
        {
            while (remaining_tasks > 0) 
            {
                wait();
            }
        }

        void done()
        {
            remaining_tasks--;
        }
    };

    struct Worker
    {
        uint32_t id = 0;
        std::thread thread;
        std::function<void()> task = nullptr;
        bool running = true;
        TaskQueue* queue = nullptr;

        Worker() = default;

        Worker(TaskQueue& queue, uint32_t id): id{id}, queue{&queue}
        {
            thread = std::thread([this](){
                run();
            });
        }

        void run()
        {
            while (running)
            {
                queue->get_task(task);
                if (task == nullptr)
                {
                    TaskQueue::wait();
                }
                else 
                {
                    task();
                    queue->done();
                    task = nullptr;
                }
            }
        }

        void stop()
        {
            running = false;
            thread.join();
        }
    };

    struct ThreadPool
    {
        uint32_t            thread_count = 0;
        TaskQueue           queue;
        std::vector<Worker> workers;

        explicit
        ThreadPool(uint32_t thread_count): thread_count{thread_count}
        {
            workers.reserve(thread_count);
            for (uint32_t i{thread_count}; i--;) 
            {
                workers.emplace_back(queue, static_cast<uint32_t>(workers.size()));
            }
        }

        virtual ~ThreadPool()
        {
            for (Worker& worker : workers) 
            {
                worker.stop();
            }
        }

        template<typename Callback_Function>
        void add_work_queue(Callback_Function&& callback)
        {
            queue.add_work_queue(std::forward<Callback_Function>(callback));
        }

        void wait_for_completion() const
        {
            queue.wait_for_completion();
        }

        template<typename Callback_Function>
        void create_work_queue(uint32_t element_count, Callback_Function&& callback)
        {
            queue.pause = true;
            std::queue<std::function<void()>> work_queue;

            const uint32_t batches = thread_count * 2;

            const uint32_t batch_size = element_count / batches;
            for (uint32_t i{0}; i < batches; ++i) 
            {
                const uint32_t start = batch_size * i;
                const uint32_t end   = start + batch_size;
                work_queue.emplace([start, end, &callback](){ callback(start, end); });
            }

            if (batch_size * batches < element_count)
            {
                const uint32_t start = batch_size * batches;
                callback(start, element_count);
            }

            add_work_queue(work_queue);
            queue.pause = false;
            wait_for_completion();
        }
    };

}