#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>


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
