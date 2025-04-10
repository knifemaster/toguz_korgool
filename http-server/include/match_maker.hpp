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
//#include <game_manager2.hpp>

struct Player {
    int client_fd;
    int id;
    int rating;
    //int fd; // filedescriptor for player // чтобы выдавать ответы обоим клиентам
    //int opponent_id;
    //int game_id;
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
    auto & get_players() {
        return players;
    }

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
    std::lock_guard<std::mutex> lock(mtx);  // Блокировка для потокобезопасности
    if (players.size() < 2) return false;   // Недостаточно игроков

    auto now = std::chrono::system_clock::now();

    for (auto it1 = players.begin(); it1 != players.end(); ++it1) {
        // Динамически увеличиваем допустимую разницу рейтинга
        auto waitTime = std::chrono::duration_cast<std::chrono::seconds>(now - it1->joinTime);
        int dynamicMaxDiff = 100 + waitTime.count() * 10;

        // Определяем границы поиска по рейтингу
        Player lowerBound{0, 0, it1->rating - dynamicMaxDiff, {}};
        Player upperBound{0, 0, it1->rating + dynamicMaxDiff, {}};

        // Находим диапазон подходящих игроков
        auto lower = players.lower_bound(lowerBound);
        auto upper = players.upper_bound(upperBound);


        //auto lower = players.lower_bound(lowerBound);
        //auto upper = players.upper_bound(upperBound);

        // Вывод границ поиска
        std::cout << "lower: ";
        if (lower != players.end()) {
            std::cout << "игрок с рейтингом " << lower->rating;
        } else {
            std::cout << "end()";
        }
        
        std::cout << ", upper: ";
        if (upper != players.end()) {
            std::cout << "игрок с рейтингом " << upper->rating;
        } else {
            std::cout << "end()";
        }
        std::cout << std::endl;

        // Подсчет игроков в диапазоне
        size_t candidates = std::distance(lower, upper);
        std::cout << "Найдено кандидатов в диапазоне: " << candidates << std::endl;


        // Ищем первого подходящего соперника
        for (auto it2 = lower; it2 != upper; ++it2) {
            if (it2 != it1) {  // Не сравниваем игрока с самим собой
                match = {*it1, *it2};
                // Важно сначала удалить it2, чтобы не инвалидировать it1
                players.erase(it2);
                players.erase(it1);
                total_matches++;
                return true;  // Успешное нахождение пары
            }
        }
    }
    return false;  // Подходящая пара не найдена
}

/*
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

            std::cout << "before for ___________" << std::endl;
            for (auto it2 = lower; it2 != upper; ++it2) {
                std::cout << +
                if (it2 != it1) {
                    match = {*it1, *it2};
                    players.erase(it1);
                    players.erase(it2);
                    total_matches++;
                    std::cout << "returning true" << std::endl;
                    return true;
                }
            }

            std::cout << "after for ___________" << std::endl;
        }
        return false;
    }
*/
    void matchingThread() {
        while (!shutdown_flag) {
            std::pair<Player, Player> match;
            if (tryFindMatch(match)) {
                // нашли игроков
                
                //std::cout << match.first.game_id << " " << match.second.game_id << std::endl;
                
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

/*
int main() {
    ThreadSafeMatchmaker matchmaker;
    
    // Запускаем поток для матчинга
    std::thread matching_thread([&]() {
        matchmaker.matchingThread();
    });

    // Симуляция добавления игроков из нескольких потоков
    std::vector<std::thread> player_threads;
    std::atomic<int> player_id{0};

    for (int i = 0; i < 5; ++i) {
        player_threads.emplace_back([&]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> rating_dist(1000, 3000);
            std::uniform_int_distribution<> delay_dist(50, 200);

            for (int j = 0; j < 200; ++j) {
                int id = ++player_id;
                matchmaker.addPlayer({
                    id,
                    rating_dist(gen),
                    std::chrono::system_clock::now()
                });
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
            }
        });
    }

    // Ждём завершения всех потоков с игроками
    for (auto& t : player_threads) {
        t.join();
    }

    // Завершаем работу матчмейкера
    matchmaker.shutdown();
    matching_thread.join();

    // Выводим итоговую статистику
    matchmaker.printStats();

    return 0;
}
*/
