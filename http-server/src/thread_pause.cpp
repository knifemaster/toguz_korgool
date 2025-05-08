#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <queue>
#include <functional>


class PausableThread {
public:
    PausableThread() : is_paused(false), should_stop(false) {}

    void start() {
        worker = std::jthread([this](std::stop_token stoken) {
            while (!should_stop && !stoken.stop_requested()) {
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    cv.wait(lock, [this, &stoken] {
                        return !is_paused || should_stop || stoken.stop_requested();
                    });
                }

                if (should_stop || stoken.stop_requested())
                    break;

                std::cout << "Работаем... (поток " << std::this_thread::get_id() << ")\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                if (is_paused) {
                    std::cout << "Поток завершил задачу и приостановлен\n";
                    std::unique_lock<std::mutex> lock(mtx);
                    cv.wait(lock, [this, &stoken] {
                        return !is_paused || should_stop || stoken.stop_requested();
                    });
                }
            }
        });
    }

    void pause() {
        std::lock_guard<std::mutex> lock(mtx);
        is_paused = true;
        std::cout << "Ожидание завершения текущей задачи...\n";
    }

    void resume() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            is_paused = false;
        }
        cv.notify_one();
        std::cout << "Поток возобновлён\n";
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            should_stop = true;
        }
        cv.notify_one();
    }


    ~PausableThread() {
        stop();
        if (worker.joinable())
            worker.join();
    }

private:
    std::jthread worker;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> is_paused;
    std::atomic<bool> should_stop;
};


class ThreadPool {
    public:
        ThreadPool(size_t num_threads) {
            for (size_t i = 0; i < num_threads; ++i) {
                workers.emplace_back([this] {
                    while (true) {
                        std::function<void()> task; 
                        {
                            std::unique_lock<std::mutex> lock(queue_mutex);
                            condition.wait(lock, [this] { return !tasks.empty() || stop; });

                            if (stop && tasks.empty()) return;

                            task = std::move(tasks.front());
                            tasks.pop();
                        }
                        task();
                    }        
                });
            }
        }

        template <class F>
        void enqueue(F&& f) {
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                tasks.emplace(std::forward<F>(f));
            }
            condition.notify_one();
        }

        ~ThreadPool() {
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                stop = true;
            }
            condition.notify_all();
            for (std::thread & worker : workers) {
                worker.join();
            }
        
        }

    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queue_mutex;
        std::condition_variable condition;
        bool stop = false;
};


int main() {

    PausableThread thread;
    thread.start();

    std::this_thread::sleep_for(std::chrono::seconds(2));

    thread.pause();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    thread.resume();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    thread.stop();

    return 0;
}
