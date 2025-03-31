#include <iostream>
#include <set>
#include <chrono>
#include <random>
#include <vector>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>



struct Player {
    int id;
    int rating;
    std::chrono::time_point<std::chrono::system_clock> joinTime;
};


class ThreadSafeMatchmaker {
    std::set<Player, bool(*)(const Player&, const Player&)> players;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> shutdown_flag{false};

    // Статистика
    std::atomic<int> total_matches{0};
    std::atomic<int> total_players{0};

    public:
    ThreadSafeMatchmaker()
        : players([](const Player& a, const Player& b) {
            return a.rating < b.rating || (a.rating == b.rating && a.joinTime < b.joinTime);
        }) {}


    void addPlayer(const Player& p) {
        std::lock_guard<std::mutex> lock(mtx);
        players.insert(p);
        total_players++;
        cv.notify_one();  // Оповещаем поток матчинга
    }
