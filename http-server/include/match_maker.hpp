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

struct PlayerComparator {
    bool operator()(const Player& a, const Player& b) const {
        return a.rating < b.rating ||
              (a.rating == b.rating && a.joinTime < b.joinTime);
    }
};

class ThreadSafeMatchmaker {
    std::set<Player, PlayerComparator> players;
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


    bool tryFindMatch(std::pair<Player, Player>& match) {
        std::lock_guard<std::mutex> lock(mtx);
        if (players.size() < 2) return false;

        auto now = std::chrono::system_clock::now();

        for (auto it1 = players.begin(); it1 != players.end(); ++it1) {
            auto waitTime = std::chrono::duration_cast<std::chrono::seconds>(now - it1->joinTime);
            int dynamicMaxDiff = 100 + waitTime.count() * 10;

            Player lowerBound{0, it1->rating - dynamicMaxDiff, {}};
            Player upperBound{0, it1->rating + dynamicMaxDiff, {}};

            auto lower = players.lower_bound(lowerBound);
            auto upper = players.upper_bound(upperBound);

            for (auto it2 = lower; it2 != upper; ++it2) {
                if (it2 != it1) {
                    match = {*it1, *it2};
                    players.erase(it1);
                    players.erase(it2);
                    total_matches++;
                    return true;
                }
            }
        }
        return false;
    }

   void matchingThread() {
        while (!shutdown_flag) {
            std::pair<Player, Player> match;
            if (tryFindMatch(match)) {
                // нашли игроков
                //
                // В реальном приложении здесь была бы логика старта игры
                continue;
            }

            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_for(lock, std::chrono::seconds(1), [this]() {
                return shutdown_flag || players.size() >= 2;
            });
        }
    }

    void shutdown() {
        shutdown_flag = true;
        cv.notify_all();
    }
    

    void printStats() const {
        std::cout << "Статистика:\n"
                  << "Всего игроков: " << total_players << "\n"
                  << "Найдено пар: " << total_matches << "\n"
                  << "В очереди: " << players.size() << std::endl;
        }
    };

