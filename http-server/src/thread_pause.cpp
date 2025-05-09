#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <queue>
#include <functional>
#include <unordered_map>
#include <string>

std::unordered_map<int, std::string> umap = { {1, "one"}, {2, "two"}, {3, "three"}};

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


#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>




struct Game {
    int id;
    std::string status;  // "waiting", "running", "finished"
    
    void update() {
        if (status == "waiting") {
            static thread_local std::mt19937 gen(std::random_device{}());
            std::uniform_int_distribution<> dis(0, 1);
            if (dis(gen) == 1) {
                status = "running";
                std::cout << "Game " << id << " started!" << std::endl;
            }
        }
        else if (status == "running") {
            static thread_local std::mt19937 gen(std::random_device{}());
            std::uniform_int_distribution<> dis(1, 3);
            if (dis(gen) == 1) {
                status = "finished";
                std::cout << "Game " << id << " finished!" << std::endl;
            }
        }
    }
};

class GameManager {
public:
    void addGame(const Game& game) {
        std::lock_guard<std::mutex> lock(mutex_);
        games_.push_back(game);
        cond_.notify_one();
    }

    void updateAllGames() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& game : games_) {
            game.update();
        }
    }

    void removeFinishedGames() {
        std::lock_guard<std::mutex> lock(mutex_);
        games_.erase(
            std::remove_if(
                games_.begin(), 
                games_.end(),
                [](const Game& g) { return g.status == "finished"; }
            ),
            games_.end()
        );
    }

    void waitForGames() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]() { return !games_.empty(); });
    }

    void printGames() const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& game : games_) {
            std::cout << "Game " << game.id << ": " << game.status << std::endl;
        }
    }

private:
    std::vector<Game> games_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
};

void gameScanner(GameManager& manager) {
    while (true) {
        manager.waitForGames();
        manager.updateAllGames();
        manager.removeFinishedGames();
        manager.printGames();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

void gameCreator(GameManager& manager) {
    static int gameId = 0;
    while (true) {
        Game newGame{gameId++, "waiting"};
        manager.addGame(newGame);
        std::cout << "Created Game " << newGame.id << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}



int main() {

    PausableThread thread;
    thread.start();

    std::this_thread::sleep_for(std::chrono::seconds(2));

    thread.pause();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    thread.resume();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    thread.stop();

    ThreadPool pool(4); 
    pool.enqueue([]() { std::cout << "Задача 1 (поток " << std::this_thread::get_id() << ")\n"; });
    pool.enqueue([]() { std::cout << "Задача 2 (поток " << std::this_thread::get_id() << ")\n"; });
    //pool.enqueue([]() { std::cout << "Задача 3 (поток " << std::this_thread::get_id() << ")\n"; });
    //pool.enqueue([]() { std::cout << "Задача 4 (поток " << std::this_thread::get_id() << ")\n"; });

    auto multiply = []() { std::cout << "multiply " << std::this_thread::get_id() <<"\n"; 
        for (const auto& [key, value] : umap) {
            std::cout << key << " " << value << std::endl;
        }
    };
    auto addition = []() { std::cout << "addition " << std::this_thread::get_id() <<"\n"; };
    pool.enqueue(multiply);
    pool.enqueue(addition);
    

    GameManager manager;
    std::thread scanner(gameScanner, std::ref(manager));
    std::thread creator(gameCreator, std::ref(manager));
    scanner.join();
    creator.join();


    return 0;
}
