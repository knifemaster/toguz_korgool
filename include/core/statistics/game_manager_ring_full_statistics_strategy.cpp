#include <iostream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <print>
#include <iomanip>
#include <algorithm>
#include <tuple>
#include <climits>

#include <map>
#include <cmath>
#include <fstream>
#include <numeric>


class Ring {
public:
    std::vector<int> top{9, 9, 9, 9, 9, 9, 9, 9, 9};  // Лунки первого игрока (индексы 0-8)
    std::vector<int> down{9, 9, 9, 9, 9, 9, 9, 9, 9}; // Лунки второго игрока (индексы 0-8)
    int top_kazan = 0;    // Казан первого игрока
    int down_kazan = 0;   // Казан второго игрока

    void print() const {
        std::cout << "Player 1 (top, kazan: " << top_kazan << "): ";
        for (size_t i = 0; i < top.size(); ++i) {
            std::cout << "[" << i << "]" << top[i] << " ";
        }
        std::cout << "\nPlayer 2 (down, kazan: " << down_kazan << "): ";
        for (size_t i = 0; i < down.size(); ++i) {
            std::cout << "[" << i << "]" << down[i] << " ";
        }
        std::cout << "\n";
    }
};

// Состояния игры
enum class GameStatus {
    WAITING_FOR_PLAYERS,    // Ожидание игроков
    IN_PROGRESS,           // Игра идет
    PLAYER1_WINS,          // Победа игрока 1
    PLAYER2_WINS,          // Победа игрока 2
    DRAW,                  // Ничья
    ABANDONED              // Игра прервана
};






class ToguzKorgoolGame {
private:
    std::shared_ptr<Ring> board;
    mutable std::mutex board_mutex;
    bool game_over = false;
    GameStatus status = GameStatus::WAITING_FOR_PLAYERS;
    int top_tuz = -1;     // Позиция туза первого игрока (-1 = нет туза)
    int down_tuz = -1;    // Позиция туза второго игрока (-1 = нет туза)
    int game_id;         // ID игры
    int move_count = 0;  // Счетчик ходов
    std::chrono::steady_clock::time_point start_time;  // Время начала игры
    std::chrono::steady_clock::time_point end_time;    // Время окончания игры

    // Статистика
    std::vector<int> top_player_kazan_gains;  // Сколько коргоолов попало в казан за каждый ход игрока 1
    std::vector<int> down_player_kazan_gains; // Сколько коргоолов попало в казан за каждый ход игрока 2
    int top_player_total_kazan_gain = 0;      // Общее количество коргоолов в казане игрока 1
    int down_player_total_kazan_gain = 0;     // Общее количество коргоолов в казане игрока 2
    int top_player_max_single_move_gain = 0;  // Максимальное количество коргоолов за один ход игрока 1
    int down_player_max_single_move_gain = 0; // Максимальное количество коргоолов за один ход игрока 2
    int top_player_min_single_move_gain = INT_MAX;  // Минимальное количество коргоолов за один ход игрока 1
    int down_player_min_single_move_gain = INT_MAX; // Минимальное количество коргоолов за один ход игрока 2
    int top_player_tuz_kazan_gain = 0;        // Сколько коргоолов попало в казан через tuz игрока 1
    int down_player_tuz_kazan_gain = 0;       // Сколько коргоолов попало в казан через tuz игрока 2

    // Структура для отслеживания последней позиции при распределении
    struct LastPosition {
        std::vector<int>* side;
        int position;
        bool is_opponent_side;
    };


    // Исправленная функция распределения коргоолов с учетом тузов
    LastPosition distribute_korgools(int korgools, int start_pos, bool is_top_player) {
        int current_pos = start_pos;
        bool current_is_top = is_top_player;
        std::vector<int>* last_side = nullptr;
        int last_pos = -1;
        int tuz_gain_this_move = 0; // Сколько коргоолов попало в казан через tuz за этот ход

        for (int i = 0; i < korgools; ++i) {
            // Движение против часовой стрелки - НАЧИНАЕМ СО СЛЕДУЮЩЕЙ ПОЗИЦИИ
            if (current_is_top) {
                // На верхней стороне движемся слева направо (0→1→2→...→8)
                current_pos++;
                if (current_pos > 8) {
                    // Переход на нижнюю сторону, начинаем с позиции 8
                    current_is_top = false;
                    current_pos = 8;
                }
            } else {
                // На нижней стороне движемся справа налево (8→7→6→...→0)  
                current_pos--;
                if (current_pos < 0) {
                    // Переход на верхнюю сторону, начинаем с позиции 0
                    current_is_top = true;
                    current_pos = 0;
                }
            }

            // Проверяем, является ли текущая позиция тузом
            bool captured_by_tuz = false;
            
            if (current_is_top) {
                // Находимся на верхней стороне (top)
                if (down_tuz == current_pos) {
                    board->down_kazan++;
                    captured_by_tuz = true;
                    tuz_gain_this_move++;
                } else if (top_tuz == current_pos) {
                    board->top_kazan++;
                    captured_by_tuz = true;
                    tuz_gain_this_move++;
                }
            } else {
                // Находимся на нижней стороне (down)
                if (top_tuz == current_pos) {
                    board->top_kazan++;
                    captured_by_tuz = true;
                    tuz_gain_this_move++;
                } else if (down_tuz == current_pos) {
                    board->down_kazan++;
                    captured_by_tuz = true;
                    tuz_gain_this_move++;
                }
            }

            if (!captured_by_tuz) {
                // Обычная лунка - добавляем коргоол
                std::vector<int>& target_side = current_is_top ? board->top : board->down;
                target_side[current_pos]++;
                
                last_side = &target_side;
                last_pos = current_pos;
            } else {
                std::vector<int>& target_side = current_is_top ? board->top : board->down;
                last_side = &target_side;
                last_pos = current_pos;
            }
        }

        // Учитываем коргоолы, попавшие через tuz
        if (is_top_player) {
            top_player_tuz_kazan_gain += tuz_gain_this_move;
        } else {
            down_player_tuz_kazan_gain += tuz_gain_this_move;
        }

        return {last_side, last_pos, 
                (is_top_player && !current_is_top) || (!is_top_player && current_is_top)};
    }

    // Проверка и обработка захвата коргоолов
    void check_capture(const LastPosition& last_pos, bool is_top_player) {
        // Захват происходит только если последний коргоол попал на сторону противника
        if (!last_pos.is_opponent_side || last_pos.side == nullptr) return;
        
        int korgools_in_hole = (*last_pos.side)[last_pos.position];
        
        // Захват при четном количестве коргоолов (и не пустой лунке)
        if (korgools_in_hole % 2 == 0 && korgools_in_hole > 0) {
            // Забираем коргоолы в казан
            if (is_top_player) {
                board->top_kazan += korgools_in_hole;
            } else {
                board->down_kazan += korgools_in_hole;
            }
            (*last_pos.side)[last_pos.position] = 0;
        }
    }

    // Исправленная функция проверки туза
    void check_tuz(const LastPosition& last_pos, bool is_top_player) {
        // Туз объявляется только если последний коргоол попал на сторону противника
        if (!last_pos.is_opponent_side || last_pos.side == nullptr) return;
        
        int korgools_in_hole = (*last_pos.side)[last_pos.position];
        
        // Условия для объявления туза:
        // 1. В лунке стало ровно 3 коргоола
        // 2. Это не 9-я лунка (индекс 8) - согласно правилам
        // 3. Игрок еще не использовал туз (проверяем по -1)
        // 4. Противник не объявил эту лунку тузом (нельзя объявить туз в лунке с тем же номером)
        if (korgools_in_hole == 3 && last_pos.position != 8) {
            // Определяем, какой игрок объявляет туз
            int& player_tuz = is_top_player ? top_tuz : down_tuz;
            int& opponent_tuz = is_top_player ? down_tuz : top_tuz;
            
            // Проверяем условия для объявления туза
            if (player_tuz == -1 && opponent_tuz != last_pos.position) {
                // Объявляем туз - присваиваем индекс лунки
                player_tuz = last_pos.position;
                
                // Забираем коргоолы из тузовой лунки
                if (is_top_player) {
                    board->top_kazan += korgools_in_hole;
                } else {
                    board->down_kazan += korgools_in_hole;
                }
                (*last_pos.side)[last_pos.position] = 0;
                
                std::cout << "TUZ declared by Player " << (is_top_player ? "1" : "2") 
                          << " at position " << last_pos.position 
                          << " on " << (last_pos.side == &board->top ? "top" : "down") << " side\n";
            }
        }
    }

    // Проверка окончания игры
    bool check_game_end() {
        // Победа по количеству коргоолов
        if (board->top_kazan >= WIN_SCORE) {
            status = GameStatus::PLAYER1_WINS;
            end_time = std::chrono::steady_clock::now();
            return true;
        }
        if (board->down_kazan >= WIN_SCORE) {
            status = GameStatus::PLAYER2_WINS;
            end_time = std::chrono::steady_clock::now();
            return true;
        }
        
        // Проверка на отсутствие коргоолов у игрока
        bool top_has_korgools = false;
        bool down_has_korgools = false;
        
        for (int i = 0; i < 9; ++i) {
            if (board->top[i] > 0) top_has_korgools = true;
            if (board->down[i] > 0) down_has_korgools = true;
        }
        
        if (!top_has_korgools || !down_has_korgools) {
            if (board->top_kazan > board->down_kazan) {
                status = GameStatus::PLAYER1_WINS;
            } else if (board->down_kazan > board->top_kazan) {
                status = GameStatus::PLAYER2_WINS;
            } else {
                status = GameStatus::DRAW;
            }
            end_time = std::chrono::steady_clock::now();
            return true;
        }
        
        // Проверка на ничью (по 81 коргоолу)
        if (board->top_kazan == 81 && board->down_kazan == 81) {
            status = GameStatus::DRAW;
            end_time = std::chrono::steady_clock::now();
            return true;
        }
        
        return false;
    }

public:
    static constexpr int WIN_SCORE = 82;

    // Конструктор с ID игры
    ToguzKorgoolGame(int id = 0) : game_id(id) {
        board = std::make_shared<Ring>();
        start_time = std::chrono::steady_clock::now();
    }


    std::shared_ptr<Ring> get_board() {
        return board;
    }

    // Начать игру (переводит из состояния ожидания в активное состояние)
    void start_game() {
        std::lock_guard<std::mutex> lock(board_mutex);
        if (status == GameStatus::WAITING_FOR_PLAYERS) {
            status = GameStatus::IN_PROGRESS;
            start_time = std::chrono::steady_clock::now();
        }
    }

    // Прервать игру
    void abandon_game() {
        std::lock_guard<std::mutex> lock(board_mutex);
        if (status == GameStatus::IN_PROGRESS || status == GameStatus::WAITING_FOR_PLAYERS) {
            status = GameStatus::ABANDONED;
            game_over = true;
            end_time = std::chrono::steady_clock::now();
        }
    }

    bool move(int move_number, int position, bool is_top_player) {
        std::lock_guard<std::mutex> lock(board_mutex);
        if (game_over || status != GameStatus::IN_PROGRESS) {
            return false;
        }

        auto& current_side = is_top_player ? board->top : board->down;
        
        // Проверка корректности хода
        if (position < 0 || position >= static_cast<int>(current_side.size()) || current_side[position] == 0) {
            return false;
        }

        int korgools = current_side[position];
        current_side[position] = 0;
        move_count++;

        // Сохраняем текущее состояние казана перед ходом
        int prev_top_kazan = board->top_kazan;
        int prev_down_kazan = board->down_kazan;

        // Распределение коргоолов с учетом тузов
        LastPosition last_pos = distribute_korgools(korgools, position, is_top_player);

        // Проверка и обработка захвата (только если последний коргоол не был захвачен тузом)
        if (last_pos.side != nullptr) {
            check_capture(last_pos, is_top_player);
            
            // Проверка и обработка туза (только если последний коргоол не был захвачен тузом)
            check_tuz(last_pos, is_top_player);
        }

        // Вычисляем сколько коргоолов попало в казан за этот ход
        int kazan_gain = 0;
        if (is_top_player) {
            kazan_gain = board->top_kazan - prev_top_kazan;
            top_player_kazan_gains.push_back(kazan_gain);
            top_player_total_kazan_gain += kazan_gain;
            if (kazan_gain > top_player_max_single_move_gain) {
                top_player_max_single_move_gain = kazan_gain;
            }
            if (kazan_gain < top_player_min_single_move_gain) {
                top_player_min_single_move_gain = kazan_gain;
            }
        } else {
            kazan_gain = board->down_kazan - prev_down_kazan;
            down_player_kazan_gains.push_back(kazan_gain);
            down_player_total_kazan_gain += kazan_gain;
            if (kazan_gain > down_player_max_single_move_gain) {
                down_player_max_single_move_gain = kazan_gain;
            }
            if (kazan_gain < down_player_min_single_move_gain) {
                down_player_min_single_move_gain = kazan_gain;
            }
        }

        // Проверка окончания игры
        game_over = check_game_end();

        return true;
    }

    // Получить список доступных ходов для игрока
    std::vector<int> get_available_moves(bool is_top_player) const {
        std::lock_guard<std::mutex> lock(board_mutex);
        std::vector<int> available_moves;
        
        const auto& current_side = is_top_player ? board->top : board->down;
        
        for (int i = 0; i < static_cast<int>(current_side.size()); ++i) {
            if (current_side[i] > 0) {
                available_moves.push_back(i);
            }
        }
        
        return available_moves;
    }

    void print_board() const {
        std::lock_guard<std::mutex> lock(board_mutex);
        std::cout << "\n=== Game ID: " << game_id << " (Status: " << status_to_string(status) << ") ===\n";
        std::cout << "Move count: " << move_count << "\n";
        board->print();
        
        // Вывод информации о тузах
        if (top_tuz != -1) {
            std::cout << "Player 1 TUZ: position " << top_tuz << "\n";
        }
        if (down_tuz != -1) {
            std::cout << "Player 2 TUZ: position " << down_tuz << "\n";
        }
    }

    bool is_game_over() const {
        std::lock_guard<std::mutex> lock(board_mutex);
        return game_over;
    }

    GameStatus get_status() const {
        std::lock_guard<std::mutex> lock(board_mutex);
        return status;
    }

    int get_move_count() const {
        std::lock_guard<std::mutex> lock(board_mutex);
        return move_count;
    }

    std::pair<int, int> get_scores() const {
        std::lock_guard<std::mutex> lock(board_mutex);
        return {board->top_kazan, board->down_kazan};
    }

    std::pair<int, int> get_tuz_positions() const {
        std::lock_guard<std::mutex> lock(board_mutex);
        return {top_tuz, down_tuz};
    }

    int get_game_id() const {
        return game_id;
    }

    // Получить длительность игры в миллисекундах
    long long get_game_duration_ms() const {
        std::lock_guard<std::mutex> lock(board_mutex);
        auto end = (game_over || status == GameStatus::ABANDONED) ? end_time : std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start_time).count();
    }

    // Получить статистику по ходам игрока 1
    std::tuple<std::vector<int>, int, int, int> get_top_player_kazan_stats() const {
        std::lock_guard<std::mutex> lock(board_mutex);
        return {top_player_kazan_gains, top_player_total_kazan_gain, 
                top_player_min_single_move_gain, top_player_max_single_move_gain};
    }

    // Получить статистику по ходам игрока 2
    std::tuple<std::vector<int>, int, int, int> get_down_player_kazan_stats() const {
        std::lock_guard<std::mutex> lock(board_mutex);
        return {down_player_kazan_gains, down_player_total_kazan_gain,
                down_player_min_single_move_gain, down_player_max_single_move_gain};
    }

    // Получить количество коргоолов, попавших через tuz
    std::pair<int, int> get_tuz_kazan_gains() const {
        std::lock_guard<std::mutex> lock(board_mutex);
        return {top_player_tuz_kazan_gain, down_player_tuz_kazan_gain};
    }

    // Преобразование статуса в строку
    static std::string status_to_string(GameStatus status) {
        switch (status) {
            case GameStatus::WAITING_FOR_PLAYERS: return "WAITING";
            case GameStatus::IN_PROGRESS: return "IN_PROGRESS";
            case GameStatus::PLAYER1_WINS: return "PLAYER1_WINS";
            case GameStatus::PLAYER2_WINS: return "PLAYER2_WINS";
            case GameStatus::DRAW: return "DRAW";
            case GameStatus::ABANDONED: return "ABANDONED";
            default: return "UNKNOWN";
        }
    }
};

// Структура для детальной статистики игры
struct GameInfo {
    int game_id;
    GameStatus status;
    int move_count;
    std::pair<int, int> scores;
    std::pair<int, int> tuz_positions;
    long long duration_ms;
    std::pair<int, int> tuz_kazan_gains;
    
    GameInfo(const ToguzKorgoolGame* game) {
        game_id = game->get_game_id();
        status = game->get_status();
        move_count = game->get_move_count();
        scores = game->get_scores();
        tuz_positions = game->get_tuz_positions();
        duration_ms = game->get_game_duration_ms();
        tuz_kazan_gains = game->get_tuz_kazan_gains();
    }
};

// Структура для общей статистики
struct GameStatistics {
    int total_games = 0;
    std::unordered_map<GameStatus, int> status_counts;
    int total_moves = 0;
    long long total_duration_ms = 0;
    int games_with_tuz = 0;
    int player1_wins = 0;
    int player2_wins = 0;
    int draws = 0;
    double avg_moves_per_game = 0.0;
    double avg_duration_ms = 0.0;
    int min_moves = INT_MAX;
    int max_moves = 0;
    long long min_duration_ms = LLONG_MAX;
    long long max_duration_ms = 0;
    
    // Новая статистика
    double avg_kazan_gain_per_move_p1 = 0.0;
    double avg_kazan_gain_per_move_p2 = 0.0;
    int max_single_move_gain_p1 = 0;
    int max_single_move_gain_p2 = 0;
    int min_single_move_gain_p1 = INT_MAX;
    int min_single_move_gain_p2 = INT_MAX;
    int total_kazan_gain_p1 = 0;
    int total_kazan_gain_p2 = 0;
    int total_tuz_kazan_gain_p1 = 0;
    int total_tuz_kazan_gain_p2 = 0;
    double tuz_kazan_percentage_p1 = 0.0;
    double tuz_kazan_percentage_p2 = 0.0;
};



class GameEvaluator {
private:
    ToguzKorgoolGame* game;
    
public:
    GameEvaluator(ToguzKorgoolGame* g) : game(g) {}
    
    // Оценка текущего состояния для игрока
    float evaluate(bool for_top_player) const {
        auto scores = game->get_scores();
        auto tuz_pos = game->get_tuz_positions();
        
        float score = 0.0f;
        
        // 1. Основная оценка по количеству коргоолов в казане
        int my_kazan = for_top_player ? scores.first : scores.second;
        int opp_kazan = for_top_player ? scores.second : scores.first;
        score += (my_kazan - opp_kazan) * 1.0f;
        
        // 2. Оценка по тузам
        int my_tuz = for_top_player ? tuz_pos.first : tuz_pos.second;
        int opp_tuz = for_top_player ? tuz_pos.second : tuz_pos.first;
        
        if (my_tuz != -1) score += 5.0f;  // Бонус за наличие туза
        if (opp_tuz != -1) score -= 3.0f; // Штраф за туз противника
        
        // 3. Оценка по распределению коргоолов на доске
        auto& my_side = for_top_player ? game->get_board()->top : game->get_board()->down;
        //game->
        auto& opp_side = for_top_player ? game->get_board()->down : game->get_board()->top;
        
        int my_total = 0;
        int opp_total = 0;
        
        for (int i = 0; i < 9; ++i) {
            my_total += my_side[i];
            opp_total += opp_side[i];
            
            // Бонус за лунки с нечетным количеством (потенциальный захват)
            if (opp_side[i] % 2 == 1 && opp_side[i] > 0) {
                score += 0.5f;
            }
        }
        
        score += (my_total - opp_total) * 0.2f;
        
        // 4. Оценка по статистике предыдущих ходов
        auto [my_gains, my_total_gain, my_min, my_max] = 
            for_top_player ? game->get_top_player_kazan_stats() : game->get_down_player_kazan_stats();
        
        if (!my_gains.empty()) {
            float avg_gain = static_cast<float>(my_total_gain) / my_gains.size();
            score += avg_gain * 0.3f;
        }
        
        return score;
    }
};


class MoveSelector {
private:
    ToguzKorgoolGame* game;
    std::mt19937 gen;
    
    struct MoveFeatures {
        int hole;
        int korgools;
        float capture_prob;
        float tuz_prob;
        float expected_gain;
    };
    
public:
    MoveSelector(ToguzKorgoolGame* g) : game(g), 
        gen(std::chrono::steady_clock::now().time_since_epoch().count()) {}
    
    // Выбор лучшего хода с учетом статистики
    int selectBestMove(bool is_top_player) {
        auto moves = game->get_available_moves(is_top_player);
        if (moves.empty()) return -1;
        
        std::vector<MoveFeatures> features;
        
        // Анализ каждого возможного хода
        for (int hole : moves) {
            MoveFeatures mf;
            mf.hole = hole;
            
            auto& side = is_top_player ? game->get_board()->top : game->get_board()->down;
            mf.korgools = side[hole];
            
            // Вероятность захвата (эвристика)
            mf.capture_prob = calculateCaptureProb(hole, is_top_player);
            
            // Вероятность получения туза (эвристика)
            mf.tuz_prob = calculateTuzProb(hole, is_top_player);
            
            // Ожидаемый выигрыш (на основе статистики)
            mf.expected_gain = calculateExpectedGain(hole, is_top_player);
            
            features.push_back(mf);
        }
        
        // Сортировка ходов по комплексной оценке
        std::sort(features.begin(), features.end(), [](const MoveFeatures& a, const MoveFeatures& b) {
            float score_a = a.expected_gain * 0.6f + a.capture_prob * 0.3f + a.tuz_prob * 0.1f;
            float score_b = b.expected_gain * 0.6f + b.capture_prob * 0.3f + b.tuz_prob * 0.1f;
            return score_a > score_b;
        });
        
        // Выбор лучшего хода (с небольшим элементом случайности для топ-3 ходов)
        int best_move = features[0].hole;
        if (features.size() > 1) {
            std::uniform_int_distribution<> dist(0, std::min(2, (int)features.size()-1));
            best_move = features[dist(gen)].hole;
        }
        
        return best_move;
    }
    
private:
    float calculateCaptureProb(int hole, bool is_top_player) {
        // Эвристика: вероятность захвата зависит от количества коргоолов и позиции
        const auto& side = is_top_player ? game->get_board()->top : game->get_board()->down;
        int korgools = side[hole];
        
        // Базовые вероятности на основе статистики
        if (korgools == 1) return 0.1f;
        if (korgools == 2) return 0.3f;
        if (korgools == 3) return 0.5f;
        if (korgools >= 4 && korgools <= 6) return 0.7f;
        return 0.4f; // Для других случаев
    }
    
    float calculateTuzProb(int hole, bool is_top_player) {
        // Вероятность получить туз при данном ходе
        // Туз можно получить только на стороне противника и не в 9-й лунке
        if (hole == 8) return 0.0f; // Нельзя в 9-й лунке
        
        auto tuz_pos = game->get_tuz_positions();
        int my_tuz = is_top_player ? tuz_pos.first : tuz_pos.second;
        
        // Если уже есть туз, новый получить нельзя
        if (my_tuz != -1) return 0.0f;
        
        const auto& side = is_top_player ? game->get_board()->top : game->get_board()->down;
        int korgools = side[hole];
        
        // Эвристика: вероятность выше для ходов с 3 коргоолами
        if (korgools == 3) return 0.4f;
        if (korgools == 2 || korgools == 4) return 0.2f;
        return 0.05f;
    }
    
    float calculateExpectedGain(int hole, bool is_top_player) {
        // Ожидаемый выигрыш на основе статистики предыдущих ходов
        auto [gains, total, min_gain, max_gain] = 
            is_top_player ? game->get_top_player_kazan_stats() : game->get_down_player_kazan_stats();
        
        if (gains.empty()) {
            // Если статистики нет, используем эвристику
            const auto& side = is_top_player ? game->get_board()->top : game->get_board()->down;
            return side[hole] * 0.5f; // Примерная эвристика
        }
        
        // Средний выигрыш за ход
        float avg_gain = static_cast<float>(total) / gains.size();
        
        // Корректировка на основе конкретной лунки
        const auto& side = is_top_player ? game->get_board()->top : game->get_board()->down;
        int korgools = side[hole];
        
        // Чем больше коргоолов, тем выше потенциальный выигрыш
        return avg_gain * (0.5f + 0.1f * korgools);
    }
};





// Класс для управления играми
class GameManager {
private:
    std::unordered_map<int, std::unique_ptr<ToguzKorgoolGame>> games;
    mutable std::mutex games_mutex;
    mutable std::mutex id_generation_mutex;
    std::mt19937 random_gen;
    
    int next_id = 1;

    // Потокобезопасная генерация уникального ID игры
    int generate_game_id() {
        std::lock_guard<std::mutex> lock(id_generation_mutex);
        return next_id++;
    }

public:
    GameManager() : random_gen(std::chrono::steady_clock::now().time_since_epoch().count()) {}

    // Установить последний ID игры из базы данных
    void set_last_game_id_from_db(int id) {
        std::lock_guard<std::mutex> lock(id_generation_mutex);
        next_id = id + 1;
    }

    // Создать новую игру
    int create_game() {
        std::lock_guard<std::mutex> lock(games_mutex);
        
        int game_id = generate_game_id();
        games[game_id] = std::make_unique<ToguzKorgoolGame>(game_id);
        
        std::cout << "Created new game with ID: " << game_id << "\n";
        return game_id;
    }


    void play_ai_game(int game_id, bool show_moves = false) {
        ToguzKorgoolGame* game = get_game(game_id);
        if (!game) {
            std::cout << "Game " << game_id << " not found!\n";
            return;
        }

        start_game(game_id);
        
        if (show_moves) {
            std::cout << "\n=== Starting AI game " << game_id << " ===\n";
            game->print_board();
        }

        bool current_player_top = true;
        GameEvaluator evaluator(game);
        MoveSelector selector(game);
        
        while (!game->is_game_over()) {
            // Выбираем ход с помощью ИИ
            int chosen_move = selector.selectBestMove(current_player_top);
            std::cout << "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW" << chosen_move << std::endl;
            
            if (chosen_move == -1) {
                if (show_moves) {
                    std::cout << "No moves available for Player " << (current_player_top ? "1" : "2") << "\n";
                }
                break;
            }

            // Выполняем ход
            bool move_success = game->move(game->get_move_count() + 1, chosen_move, current_player_top);
            
            if (move_success) {
                if (show_moves) {
                    std::cout << "Player " << (current_player_top ? "1" : "2") 
                              << " moves from hole " << chosen_move << "\n";
                    
                    auto scores = game->get_scores();
                    std::cout << "Scores: P1:" << scores.first << " P2:" << scores.second << "\n";
                    
                    // Показываем оценку состояния
                    float eval = evaluator.evaluate(current_player_top);
                    std::cout << "Position evaluation: " << eval << "\n";
                }
                
                // Меняем игрока
                current_player_top = !current_player_top;
            } else {
                if (show_moves) {
                    std::cout << "Move failed!\n";
                }
                break;
            }
        }

        if (show_moves) {
            std::cout << "\n=== Game " << game_id << " finished ===\n";
            game->print_board();
            std::cout << "Final status: " << ToguzKorgoolGame::status_to_string(game->get_status()) << "\n";
        }
    }

    // Получить игру по ID
    ToguzKorgoolGame* get_game(int game_id) {
        std::lock_guard<std::mutex> lock(games_mutex);
        
        auto it = games.find(game_id);
        if (it != games.end()) {
            return it->second.get();
        }
        
        return nullptr;
    }

    // Начать игру
    bool start_game(int game_id) {
        std::lock_guard<std::mutex> lock(games_mutex);
        
        auto it = games.find(game_id);
        if (it != games.end()) {
            it->second->start_game();
            return true;
        }
        
        return false;
    }

    // Прервать игру
    bool abandon_game(int game_id) {
        std::lock_guard<std::mutex> lock(games_mutex);
        
        auto it = games.find(game_id);
        if (it != games.end()) {
            it->second->abandon_game();
            return true;
        }
        
        return false;
    }

    // Получить статус игры
    GameStatus get_game_status(int game_id) const {
        std::lock_guard<std::mutex> lock(games_mutex);
        
        auto it = games.find(game_id);
        if (it != games.end()) {
            return it->second->get_status();
        }
        
        return GameStatus::ABANDONED;
    }

    // Удалить игру
    bool remove_game(int game_id) {
        std::lock_guard<std::mutex> lock(games_mutex);
        
        auto it = games.find(game_id);
        if (it != games.end()) {
            games.erase(it);
            std::cout << "Game with ID " << game_id << " removed.\n";
            return true;
        }
        
        return false;
    }

    // Получить список всех игр
    std::vector<int> get_all_game_ids() const {
        std::lock_guard<std::mutex> lock(games_mutex);
        
        std::vector<int> game_ids;
        game_ids.reserve(games.size());
        
        for (const auto& pair : games) {
            game_ids.push_back(pair.first);
        }
        
        return game_ids;
    }

    // НОВЫЙ МЕТОД: Получить все игры по статусу
    std::vector<GameInfo> get_all_games_by_status(GameStatus status) const {
        std::lock_guard<std::mutex> lock(games_mutex);
        
        std::vector<GameInfo> games_info;
        
        for (const auto& pair : games) {
            if (pair.second->get_status() == status) {
                games_info.emplace_back(pair.second.get());
            }
        }
        
        return games_info;
    }

    // УЛУЧШЕННЫЙ МЕТОД: Получить игры по статусу (только ID)
    std::vector<int> get_games_by_status(GameStatus status) const {
        std::lock_guard<std::mutex> lock(games_mutex);
        
        std::vector<int> game_ids;
        for (const auto& pair : games) {
            if (pair.second->get_status() == status) {
                game_ids.push_back(pair.first);
            }
        }
        
        return game_ids;
    }

    // НОВЫЙ МЕТОД: Получить детальную информацию о всех играх
    std::vector<GameInfo> get_all_games_info() const {
        std::lock_guard<std::mutex> lock(games_mutex);
        
        std::vector<GameInfo> games_info;
        games_info.reserve(games.size());
        
        for (const auto& pair : games) {
            games_info.emplace_back(pair.second.get());
        }
        
        return games_info;
    }

    // НОВЫЙ МЕТОД: Получить общую статистику
    GameStatistics get_statistics() const {
        std::lock_guard<std::mutex> lock(games_mutex);
        
        GameStatistics stats;
        stats.total_games = games.size();
        
        if (stats.total_games == 0) {
            return stats;
        }
        
        int total_moves_p1 = 0;
        int total_moves_p2 = 0;
        
        for (const auto& pair : games) {
            const auto* game = pair.second.get();
            GameStatus status = game->get_status();
            int moves = game->get_move_count();
            long long duration = game->get_game_duration_ms();
            auto tuz_pos = game->get_tuz_positions();
            
            // Подсчет по статусам
            stats.status_counts[status]++;
            
            // Подсчет побед
            if (status == GameStatus::PLAYER1_WINS) stats.player1_wins++;
            else if (status == GameStatus::PLAYER2_WINS) stats.player2_wins++;
            else if (status == GameStatus::DRAW) stats.draws++;
            
            // Подсчет игр с тузами
            if (tuz_pos.first != -1 || tuz_pos.second != -1) {
                stats.games_with_tuz++;
            }
            
            // Суммарные статистики
            stats.total_moves += moves;
            stats.total_duration_ms += duration;
            
            // Минимальные и максимальные значения
            if (moves > 0) {  // Только для начатых игр
                stats.min_moves = std::min(stats.min_moves, moves);
                stats.max_moves = std::max(stats.max_moves, moves);
            }
            
            stats.min_duration_ms = std::min(stats.min_duration_ms, duration);
            stats.max_duration_ms = std::max(stats.max_duration_ms, duration);
            
            // Статистика по казану
            auto [top_gains, top_total, top_min, top_max] = game->get_top_player_kazan_stats();
            auto [down_gains, down_total, down_min, down_max] = game->get_down_player_kazan_stats();
            auto tuz_gains = game->get_tuz_kazan_gains();
            
            stats.total_kazan_gain_p1 += top_total;
            stats.total_kazan_gain_p2 += down_total;
            
            if (top_max > stats.max_single_move_gain_p1) {
                stats.max_single_move_gain_p1 = top_max;
            }
            if (down_max > stats.max_single_move_gain_p2) {
                stats.max_single_move_gain_p2 = down_max;
            }
            
            if (top_min < stats.min_single_move_gain_p1) {
                stats.min_single_move_gain_p1 = top_min;
            }
            if (down_min < stats.min_single_move_gain_p2) {
                stats.min_single_move_gain_p2 = down_min;
            }
            
            stats.total_tuz_kazan_gain_p1 += tuz_gains.first;
            stats.total_tuz_kazan_gain_p2 += tuz_gains.second;
            
            total_moves_p1 += top_gains.size();
            total_moves_p2 += down_gains.size();

            std::cout << "************top***************************************" << std::endl;
            for (const auto& gain : top_gains) {
                std::cout << gain << " ";
            }
            std::cout << std::endl;

            std::cout << "************down*************************************" << std::endl;
            for (const auto& gain : down_gains) {
                std::cout << gain << " ";
            }
            std::cout << std::endl;


        }
        
        // Средние значения
        stats.avg_moves_per_game = static_cast<double>(stats.total_moves) / stats.total_games;
        stats.avg_duration_ms = static_cast<double>(stats.total_duration_ms) / stats.total_games;
        
        // Средний выигрыш за ход
        if (total_moves_p1 > 0) {
            stats.avg_kazan_gain_per_move_p1 = static_cast<double>(stats.total_kazan_gain_p1) / total_moves_p1;
        }
        if (total_moves_p2 > 0) {
            stats.avg_kazan_gain_per_move_p2 = static_cast<double>(stats.total_kazan_gain_p2) / total_moves_p2;
        }
        
        // Процент коргоолов через tuz
        if (stats.total_kazan_gain_p1 > 0) {
            stats.tuz_kazan_percentage_p1 = (static_cast<double>(stats.total_tuz_kazan_gain_p1) / stats.total_kazan_gain_p1) * 100;
        }
        if (stats.total_kazan_gain_p2 > 0) {
            stats.tuz_kazan_percentage_p2 = (static_cast<double>(stats.total_tuz_kazan_gain_p2) / stats.total_kazan_gain_p2) * 100;
        }
        
        // Если нет завершенных игр
        if (stats.min_moves == INT_MAX) stats.min_moves = 0;
        if (stats.min_duration_ms == LLONG_MAX) stats.min_duration_ms = 0;
        if (stats.min_single_move_gain_p1 == INT_MAX) stats.min_single_move_gain_p1 = 0;
        if (stats.min_single_move_gain_p2 == INT_MAX) stats.min_single_move_gain_p2 = 0;
        
        return stats;
    }

    // НОВЫЙ МЕТОД: Показать игры по статусам с детальной информацией
    void show_games_by_status() const {
        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "=== GAMES BY STATUS (DETAILED) ===\n";
        std::cout << std::string(80, '=') << "\n";
        
        // Получаем все возможные статусы
        std::vector<GameStatus> all_statuses = {
            GameStatus::WAITING_FOR_PLAYERS,
            GameStatus::IN_PROGRESS,
            GameStatus::PLAYER1_WINS,
            GameStatus::PLAYER2_WINS,
            GameStatus::DRAW,
            GameStatus::ABANDONED
        };
        
        for (GameStatus status : all_statuses) {
            auto games_with_status = get_all_games_by_status(status);
            
            std::cout << "\n📊 " << ToguzKorgoolGame::status_to_string(status) 
                      << " (" << games_with_status.size() << " games)\n";
            std::cout << std::string(50, '-') << "\n";
            
            if (games_with_status.empty()) {
                std::cout << "   No games with this status\n";
                continue;
            }
            
            // Заголовок таблицы
            std::cout << std::left 
                      << std::setw(8) << "ID"
                      << std::setw(8) << "Moves"
                      << std::setw(12) << "Duration"
                      << std::setw(15) << "Score(P1:P2)"
                      << std::setw(12) << "TUZ(P1:P2)"
                      << std::setw(15) << "TUZ Kazan(P1:P2)"
                      << "\n";
            std::cout << std::string(70, '-') << "\n";
            
            // Сортируем по ID игры
            std::sort(games_with_status.begin(), games_with_status.end(),
                     [](const GameInfo& a, const GameInfo& b) {
                         return a.game_id < b.game_id;
                     });
            
            for (const auto& game_info : games_with_status) {
                std::cout << std::left 
                          << std::setw(8) << game_info.game_id
                          << std::setw(8) << game_info.move_count
                          << std::setw(12) << (game_info.duration_ms / 1000.0) << "s"
                          << std::setw(15) << (std::to_string(game_info.scores.first) + ":" + std::to_string(game_info.scores.second))
                          << std::setw(12) << (std::to_string(game_info.tuz_positions.first) + ":" + std::to_string(game_info.tuz_positions.second))
                          << std::setw(15) << (std::to_string(game_info.tuz_kazan_gains.first) + ":" + std::to_string(game_info.tuz_kazan_gains.second))
                          << "\n";
            }
        }
    }

    void show_comprehensive_statistics() const {
        GameStatistics stats = get_statistics();
        
        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "=== COMPREHENSIVE GAME STATISTICS ===\n";
        std::cout << std::string(80, '=') << "\n";
        
        // Общая информация
        std::cout << "\n🎮 GENERAL STATISTICS:\n";
        std::cout << std::string(40, '-') << "\n";
        std::cout << "Total games:           " << stats.total_games << "\n";
        std::cout << "Total moves:           " << stats.total_moves << "\n";
        std::cout << "Min moves:             " << stats.min_moves << "\n";
        std::cout << "Max moves:             " << stats.max_moves << "\n";
        std::cout << "AVG moves per game:    " << stats.avg_moves_per_game << "\n";
        std::cout << "Player 1 wins:         " << stats.player1_wins << "\n";
        std::cout << "Player 2 wins:         " << stats.player2_wins << "\n";
        std::cout << "Draw games:            " << stats.draws << "\n";
        std::cout << "Games with TUZ:        " << stats.games_with_tuz << "\n";
        std::cout << "Max duration:          " << stats.max_duration_ms << " ms\n";
        std::cout << "Min duration:          " << stats.min_duration_ms << " ms\n";
        std::cout << "Total Duration:        " << (stats.total_duration_ms / 1000.0) << " seconds\n";
        std::cout << "AVG Duration:          " << stats.avg_duration_ms << " ms\n";
        
        for (auto& count : stats.status_counts) {
            std::cout << ToguzKorgoolGame::status_to_string(count.first) << ": " << count.second << "\n";
        }
        
        // Статистика по казану
        std::cout << "\n💰 KAZAN STATISTICS:\n";
        std::cout << std::string(40, '-') << "\n";
        std::cout << "Total kazan gain P1:  " << stats.total_kazan_gain_p1 << "\n";
        std::cout << "Total kazan gain P2:  " << stats.total_kazan_gain_p2 << "\n";
        std::cout << "AVG kazan gain/move P1: " << stats.avg_kazan_gain_per_move_p1 << "\n";
        std::cout << "AVG kazan gain/move P2: " << stats.avg_kazan_gain_per_move_p2 << "\n";
        
        // Статистика по ходам
        std::cout << "\n🎯 MOVE GAIN STATISTICS:\n";
        std::cout << std::string(40, '-') << "\n";
        std::cout << "Min single move gain P1: " << stats.min_single_move_gain_p1 << "\n";
        std::cout << "Min single move gain P2: " << stats.min_single_move_gain_p2 << "\n";
        std::cout << "Max single move gain P1: " << stats.max_single_move_gain_p1 << "\n";
        std::cout << "Max single move gain P2: " << stats.max_single_move_gain_p2 << "\n";
        
        // Статистика по tuz
        std::cout << "\n🎲 TUZ STATISTICS:\n";
        std::cout << std::string(40, '-') << "\n";
        std::cout << "Total kazan via tuz P1: " << stats.total_tuz_kazan_gain_p1 
                  << " (" << std::fixed << std::setprecision(1) << stats.tuz_kazan_percentage_p1 << "% of total)\n";
        std::cout << "Total kazan via tuz P2: " << stats.total_tuz_kazan_gain_p2 
                  << " (" << std::fixed << std::setprecision(1) << stats.tuz_kazan_percentage_p2 << "% of total)\n";
        std::cout.unsetf(std::ios::fixed);

        
        //for (const auto& move_grade : stats)

    }

    void play_automatic_game(int game_id, bool show_moves = false) {
        ToguzKorgoolGame* game = get_game(game_id);
        if (!game) {
            std::cout << "Game " << game_id << " not found!\n";
            return;
        }

        start_game(game_id);
        
        if (show_moves) {
            std::cout << "\n=== Starting automatic game " << game_id << " ===\n";
            game->print_board();
        }

        bool current_player_top = true;
        std::uniform_int_distribution<> delay_dist(1, 5);

        


        while (!game->is_game_over()) {
            // Получаем доступные ходы для текущего игрока
            std::vector<int> available_moves = game->get_available_moves(current_player_top);
            
            if (available_moves.empty()) {
                if (show_moves) {
                    std::cout << "No moves available for Player " << (current_player_top ? "1" : "2") << "\n";
                }
                break;
            }

            // Выбираем случайный ход
            std::uniform_int_distribution<> move_dist(0, available_moves.size() - 1);

            int chosen_move = available_moves[move_dist(random_gen)];

            // Выполняем ход
            bool move_success = game->move(game->get_move_count() + 1, chosen_move, current_player_top);
            
            if (move_success) {
                if (show_moves) {
                    std::cout << "Player " << (current_player_top ? "1" : "2") 
                              << " moves from hole " << chosen_move << "\n";
                    
                    auto scores = game->get_scores();
                    std::cout << "Scores: P1:" << scores.first << " P2:" << scores.second << "\n";
                }
                
                // Меняем игрока
                current_player_top = !current_player_top;
                
                // Небольшая задержка для наглядности
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(random_gen)));
            } else {
                if (show_moves) {
                    std::cout << "Move failed!\n";
                }
                break;
            }
        }

        if (show_moves) {
            std::cout << "\n=== Game " << game_id << " finished ===\n";
            game->print_board();
            std::cout << "Final status: " << ToguzKorgoolGame::status_to_string(game->get_status()) << "\n";
        }
    }

    void show_games_stats() const {
        std::lock_guard<std::mutex> lock(games_mutex);
        
        std::cout << "\n=== Games Statistics ===\n";
        std::cout << "Total games: " << games.size() << "\n";
        
        std::unordered_map<GameStatus, int> status_count;
        for (const auto& pair : games) {
            status_count[pair.second->get_status()]++;
        }
        
        std::cout << "Games by status:\n";
        for (const auto& [status, count] : status_count) {
            std::cout << "  " << ToguzKorgoolGame::status_to_string(status) << ": " << count << "\n";
        }
        
        if (!games.empty()) {
            std::cout << "\nDetailed game info:\n";
            for (const auto& pair : games) {
                auto scores = pair.second->get_scores();
                GameStatus status = pair.second->get_status();
                std::cout << "  Game " << pair.first 
                          << " - Status: " << ToguzKorgoolGame::status_to_string(status)
                          << " - Moves: " << pair.second->get_move_count()
                          << " - Score: P1:" << scores.first << " P2:" << scores.second << "\n";
            }
        }
    }

    void remove_finished_games() {
        std::lock_guard<std::mutex> lock(games_mutex);
        
        auto it = games.begin();
        while (it != games.end()) {
            GameStatus status = it->second->get_status();
            if (status != GameStatus::WAITING_FOR_PLAYERS && status != GameStatus::IN_PROGRESS) {
                std::cout << "Removing finished game: " << it->first << " (Status: " 
                          << ToguzKorgoolGame::status_to_string(status) << ")\n";
                it = games.erase(it);
            } else {
                ++it;
            }
        }
    }

    void create_and_play_multiple_games(int num_games, bool show_moves = false) {
        std::cout << "\n=== Creating and playing " << num_games << " automatic games ===\n";
        
        std::vector<int> game_ids;
        
        // Создаем игры
        for (int i = 0; i < num_games; ++i) {
            int game_id = create_game();
            game_ids.push_back(game_id);
        }
        
        // Запускаем игры
        for (int game_id : game_ids) {
            if (show_moves) {
                std::cout << "\n--- Playing game " << game_id << " ---\n";
            }
            play_automatic_game(game_id, show_moves);
        }
        
        std::cout << "\n=== All games completed ===\n";
        show_games_stats();
    }
};



// Структура для хранения параметров модели
struct ModelParameters {
    // Веса для оценки позиции
    float kazan_weight = 1.0f;
    float tuz_weight = 5.0f;
    float hole_odd_weight = 0.5f;
    float material_weight = 0.2f;
    float move_gain_weight = 0.3f;
    
    // Параметры для MoveSelector
    float expected_gain_weight = 0.6f;
    float capture_prob_weight = 0.3f;
    float tuz_prob_weight = 0.1f;
    
    // Пороги для принятия решений
    float tuz_declare_threshold = 0.4f;
    float risk_threshold = 0.7f;
    
    void save(std::ofstream& out) const {
        out.write(reinterpret_cast<const char*>(this), sizeof(*this));
    }
    
    void load(std::ifstream& in) {
        in.read(reinterpret_cast<char*>(this), sizeof(*this));
    }
};

class NeuralNetwork {
private:
    std::vector<std::vector<float>> weights;
    std::vector<std::vector<float>> biases;
    
public:
    NeuralNetwork(const std::vector<int>& layer_sizes) {
        for (size_t i = 1; i < layer_sizes.size(); ++i) {
            int prev_size = layer_sizes[i-1];
            int curr_size = layer_sizes[i];
            
            // Инициализация весов (Xavier/Glorot initialization)
            std::vector<float> layer_weights(curr_size * prev_size);
            float stddev = sqrtf(2.0f / (prev_size + curr_size));
            std::normal_distribution<float> dist(0.0f, stddev);
            std::mt19937 gen(std::random_device{}());
            
            for (float& w : layer_weights) {
                w = dist(gen);
            }
            
            weights.push_back(layer_weights);
            
            // Инициализация смещений нулями
            biases.emplace_back(curr_size, 0.0f);
        }
    }
    
    std::vector<float> predict(const std::vector<float>& input) {
        std::vector<float> activations = input;
        
        for (size_t i = 0; i < weights.size(); ++i) {
        
            const auto& layer_weights = weights[i];
            const auto& layer_biases = biases[i];
        
            std::cout << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%" << std::endl;
            std::println("{}", layer_weights);
            std::println("{}", layer_biases);

            std::cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" << std::endl;
            int prev_size = activations.size();
            int curr_size = layer_biases.size();
            
            std::vector<float> new_activations(curr_size);
            
            for (int j = 0; j < curr_size; ++j) {
                float sum = layer_biases[j];
                
                for (int k = 0; k < prev_size; ++k) {
                    sum += layer_weights[j * prev_size + k] * activations[k];
                }
                
                // ReLU активация
                new_activations[j] = std::max(0.0f, sum);
            }
            
            activations = std::move(new_activations);
        }
        
        // Softmax для выходного слоя
        if (!activations.empty()) {
            float max_val = *std::max_element(activations.begin(), activations.end());
            float sum = 0.0f;
            
            for (float& val : activations) {
                val = expf(val - max_val);
                sum += val;
            }
            
            for (float& val : activations) {
                val /= sum;
            }
        }
        
        return activations;
    }
    
    void train(const std::vector<std::vector<float>>& inputs,
              const std::vector<std::vector<float>>& targets,
              int epochs, float learning_rate) {
        // Реализация обратного распространения ошибки
        // (упрощенная версия для демонстрации)
        for (int epoch = 0; epoch < epochs; ++epoch) {
            for (size_t i = 0; i < inputs.size(); ++i) {
                // Прямой проход и расчет градиентов
                // Обратный проход и обновление весов
                // (полная реализация требует больше кода)
            }
        }
    }
    
    void save(const std::string& filename) {
        std::ofstream out(filename, std::ios::binary);

        // Реализация сохранения весов
    }
    
    void load(const std::string& filename) {
        std::ifstream in(filename, std::ios::binary);
        // Реализация загрузки весов
    }
};

class PositionDatabase {
private:
    std::map<std::string, std::pair<float, int>> positions; // Хэш позиции -> (оценка, частота)
    
public:
    void add_position(const std::string& hash, float evaluation) {
        auto it = positions.find(hash);
        if (it != positions.end()) {
            // Обновляем среднюю оценку
            float old_avg = it->second.first;
            int count = it->second.second;
            it->second.first = (old_avg * count + evaluation) / (count + 1);
            it->second.second = count + 1;
        } else {
            positions[hash] = {evaluation, 1};
        }
    }
    
    float get_position_evaluation(const std::string& hash) const {
        auto it = positions.find(hash);
        return it != positions.end() ? it->second.first : 0.0f;
    }
    
    void save(const std::string& filename) {
        std::ofstream out(filename);
        std::cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@save@@@@@@@@@@@@@@@@@" << std::endl;
        for (const auto& entry : positions) {
            std::cout << entry.first << " " << entry.second.first << " " << entry.second.second << std::endl;
            out << entry.first << " " << entry.second.first << " " << entry.second.second << "\n";
        }
    }
    
    void load(const std::string& filename) {
        std::cout << "___________________________LOAD________________________________" << std::endl;
        std::ifstream in(filename);
        std::string hash;
        float eval;
        int count;
        
        while (in >> hash >> eval >> count) {
            positions[hash] = {eval, count};
        }
    }
};

class AIModelTrainer {
private:
    const GameStatistics& stats;
    ModelParameters params;
    NeuralNetwork nn;
    PositionDatabase position_db;
    
public:
    AIModelTrainer(const GameStatistics& s) 
        : stats(s), nn({18, 64, 32, 1}) { // 18 входов (9 top + 9 down), 2 скрытых слоя, 1 выход
    }
    
    void train() {
        analyze_kazan_gains();
        analyze_tuz_impact();
        analyze_move_patterns();
        analyze_position_evaluations();
        train_neural_network();
        optimize_parameters();
    }
    
    void save_model(const std::string& filename) {
        std::ofstream out(filename, std::ios::binary);
        params.save(out);
        nn.save(filename + ".nn");
        position_db.save(filename + ".db");
    }
    
    void load_model(const std::string& filename) {
        std::ifstream in(filename, std::ios::binary);
        params.load(in);
        nn.load(filename + ".nn");
        position_db.load(filename + ".db");
    }
    
private:
    void analyze_kazan_gains() {
        // Анализ статистики по казану для корректировки весов
        float p1_avg = stats.avg_kazan_gain_per_move_p1;
        float p2_avg = stats.avg_kazan_gain_per_move_p2;
        
        // Если один игрок получает значительно больше, корректируем веса
        if (p1_avg > p2_avg * 1.2f) {
            params.move_gain_weight *= 1.1f;
        } else if (p2_avg > p1_avg * 1.2f) {
            params.move_gain_weight *= 0.9f;
        }
        
        std::cout << "Adjusted move gain weight to: " << params.move_gain_weight << "\n";
    }
    
    void analyze_tuz_impact() {
        // Анализ влияния туза на игру
        float p1_tuz_percent = stats.tuz_kazan_percentage_p1;
        float p2_tuz_percent = stats.tuz_kazan_percentage_p2;
        
        // Корректировка важности туза
        float avg_tuz_impact = (p1_tuz_percent + p2_tuz_percent) / 200.0f; // нормализуем к 0-1
        
        if (avg_tuz_impact > 0.15f) { // Если туз дает более 15% коргоолов
            params.tuz_weight *= 1.2f;
            params.tuz_prob_weight *= 1.1f;
        } else {
            params.tuz_weight *= 0.9f;
            params.tuz_prob_weight *= 0.9f;
        }
        
        // Корректировка порога объявления туза
        params.tuz_declare_threshold = std::clamp(avg_tuz_impact * 2.0f, 0.2f, 0.6f);
        
        std::cout << "Adjusted tuz weights - tuz_weight: " << params.tuz_weight 
                  << ", tuz_prob_weight: " << params.tuz_prob_weight
                  << ", declare_threshold: " << params.tuz_declare_threshold << "\n";
    }
    
    void analyze_move_patterns() {
        // Анализ паттернов ходов
        int p1_max_gain = stats.max_single_move_gain_p1;
        int p2_max_gain = stats.max_single_move_gain_p2;
        
        // Если максимальный выигрыш за ход слишком большой, увеличиваем важность вероятности захвата
        if (std::max(p1_max_gain, p2_max_gain) > 15) {
            params.capture_prob_weight *= 1.2f;
            params.expected_gain_weight *= 0.9f;
        }
        
        std::cout << "Adjusted move selection weights - capture_prob: " << params.capture_prob_weight
                  << ", expected_gain: " << params.expected_gain_weight << "\n";
    }
    
    void analyze_position_evaluations() {
        // Анализ оценок позиций из сыгранных игр
        // (в реальной реализации здесь бы использовались данные из логов игр)
        
        // Пример: если в выигранных позициях часто встречались нечетные лунки у противника,
        // увеличиваем вес этой характеристики
        if (stats.player1_wins > stats.player2_wins * 1.5f) {
            params.hole_odd_weight *= 1.1f;
        } else if (stats.player2_wins > stats.player1_wins * 1.5f) {
            params.hole_odd_weight *= 0.9f;
        }
    }
    
    void train_neural_network() {
        // Подготовка данных для обучения нейросети
        std::vector<std::vector<float>> inputs;
        std::vector<std::vector<float>> targets;
        
        // (в реальной реализации здесь бы использовались данные из логов игр)
        // Примерные данные для демонстрации:
        
        // Позиция с преимуществом игрока 1
        inputs.push_back({9,9,9,9,9,9,9,9,9,  // top
                          9,9,9,9,9,9,9,9,9}); // down
        targets.push_back({1.0f});              // Оценка 1.0 - сильное преимущество
        
        // Позиция с преимуществом игрока 2
        inputs.push_back({0,0,0,0,0,0,0,0,0,
                          9,9,9,9,9,9,9,9,9});
        targets.push_back({-1.0f});             // Оценка -1.0 - сильное преимущество противника
        
        // Обучаем нейросеть
        nn.train(inputs, targets, 10, 0.01f);
    }
    
    void optimize_parameters() {
        // Оптимизация параметров с помощью простого генетического алгоритма
        constexpr int population_size = 20;
        constexpr int generations = 5;
        constexpr float mutation_rate = 0.1f;
        
        std::vector<ModelParameters> population(population_size, params);
        std::vector<float> fitness(population_size);
        
        std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::normal_distribution<float> mutation_dist(0.0f, 0.1f);
        
        for (int gen_num = 0; gen_num < generations; ++gen_num) {
            // Оценка каждого индивидуума
            for (int i = 0; i < population_size; ++i) {
                fitness[i] = evaluate_parameters(population[i]);
            }
            
            // Селекция и скрещивание
            std::vector<ModelParameters> new_population;
            
            // Элитизм - сохраняем лучшего
            int best_idx = std::max_element(fitness.begin(), fitness.end()) - fitness.begin();
            new_population.push_back(population[best_idx]);
            
            while (new_population.size() < population_size) {
                // Турнирная селекция
                int a = rand() % population_size;
                int b = rand() % population_size;
                int parent1 = fitness[a] > fitness[b] ? a : b;
                
                a = rand() % population_size;
                b = rand() % population_size;
                int parent2 = fitness[a] > fitness[b] ? a : b;
                
                // Скрещивание
                ModelParameters child = crossover(population[parent1], population[parent2]);
                
                // Мутация
                if (dist(gen) < mutation_rate) child.kazan_weight += mutation_dist(gen);
                if (dist(gen) < mutation_rate) child.tuz_weight += mutation_dist(gen);
                if (dist(gen) < mutation_rate) child.hole_odd_weight += mutation_dist(gen);
                
                new_population.push_back(child);
            }
            
            population = std::move(new_population);
        }
        
        // Выбираем лучшие параметры
        params = population[0];
    }
    
    float evaluate_parameters(const ModelParameters& test_params) {
        // Оценка параметров на исторических данных
        // (в реальной реализации здесь бы использовались сохраненные игры)
        
        float score = 0.0f;
        
        // Примерная оценка - чем выше веса, тем лучше (упрощенно)
        score += test_params.kazan_weight;
        score += test_params.tuz_weight * 0.5f;
        score += test_params.hole_odd_weight * 0.3f;
        
        return score;
    }
    
    ModelParameters crossover(const ModelParameters& p1, const ModelParameters& p2) {
        ModelParameters child;
        
        // Усреднение параметров
        child.kazan_weight = (p1.kazan_weight + p2.kazan_weight) / 2;
        child.tuz_weight = (p1.tuz_weight + p2.tuz_weight) / 2;
        child.hole_odd_weight = (p1.hole_odd_weight + p2.hole_odd_weight) / 2;
        child.material_weight = (p1.material_weight + p2.material_weight) / 2;
        child.move_gain_weight = (p1.move_gain_weight + p2.move_gain_weight) / 2;
        
        return child;
    }
};

class ReinforcementTrainer {
private:
    GameManager& game_manager;
    AIModelTrainer& model_trainer;
    int episodes;
    
public:
    ReinforcementTrainer(GameManager& gm, AIModelTrainer& mt, int ep = 1000)
        : game_manager(gm), model_trainer(mt), episodes(ep) {}
    
    void train() {
        for (int ep = 0; ep < episodes; ++ep) {
            // Создаем новую игру
            int game_id = game_manager.create_game();
            ToguzKorgoolGame* game = game_manager.get_game(game_id);
            game_manager.start_game(game_id);
            
            // Играем с текущей моделью
            play_episode(game_id);
            
            // Анализируем результаты
            analyze_episode(game_id);
            
            // Периодически сохраняем модель
            if (ep % 100 == 0) {
                model_trainer.save_model("rl_checkpoint_" + std::to_string(ep) + ".bin");
            }
        }
    }
    
private:
    void play_episode(int game_id) {
        ToguzKorgoolGame* game = game_manager.get_game(game_id);
        GameEvaluator evaluator(game);
        MoveSelector selector(game);
        
        bool current_player_top = true;
        
        while (!game->is_game_over()) {
            int chosen_move = selector.selectBestMove(current_player_top);
            
            if (chosen_move == -1) break;
            
            // Сохраняем состояние до хода
            auto board_before = *game->get_board();
            float eval_before = evaluator.evaluate(current_player_top);
            
            // Делаем ход
            game->move(game->get_move_count() + 1, chosen_move, current_player_top);
            
            // Сохраняем состояние после хода
            float eval_after = evaluator.evaluate(current_player_top);
            
            // Сохраняем опыт для обучения
            save_experience(board_before, chosen_move, eval_before, eval_after, 
                          current_player_top, game->is_game_over());
            
            current_player_top = !current_player_top;
        }
    }
    
    void save_experience(const Ring& state, int action, float eval_before, 
                        float eval_after, bool is_top_player, bool is_terminal) {
        // В реальной реализации здесь бы сохранялись данные для обучения с подкреплением
        // (state, action, reward, new_state, is_terminal)
    }
    
    void analyze_episode(int game_id) {
        ToguzKorgoolGame* game = game_manager.get_game(game_id);
        GameStatus status = game->get_status();
        
        // Определяем награды в зависимости от результата
        float reward_p1 = 0.0f;
        float reward_p2 = 0.0f;
        
        switch (status) {
            case GameStatus::PLAYER1_WINS:
                reward_p1 = 1.0f;
                reward_p2 = -1.0f;
                break;
            case GameStatus::PLAYER2_WINS:
                reward_p1 = -1.0f;
                reward_p2 = 1.0f;
                break;
            case GameStatus::DRAW:
                reward_p1 = 0.1f;
                reward_p2 = 0.1f;
                break;
            default:
                break;
        }
        
        // Обновляем модель на основе наград
        // (в реальной реализации здесь бы использовался алгоритм Q-learning или Policy Gradient)
    }
};



int main() {

    GameManager game_manager;
    
    // 1. Создаем и играем автоматические игры для сбора статистики
    game_manager.create_and_play_multiple_games(100, false);
    
    // 2. Анализируем статистику
    GameStatistics stats = game_manager.get_statistics();
    AIModelTrainer trainer(stats);
    
    // 3. Базовое обучение на статистике
    trainer.train();
    trainer.save_model("toguz_korgool_model_v1.bin");
    
    // 4. Обучение с подкреплением
    ReinforcementTrainer rl_trainer(game_manager, trainer, 500);
    rl_trainer.train();
    
    // 5. Тестируем обученную модель
    int ai_game_id = game_manager.create_game();
    game_manager.play_ai_game(ai_game_id, true);
    
    // 6. Сохраняем финальную модель
    trainer.save_model("toguz_korgool_model_final.bin");

    /*
    GameManager game_manager;
    
    std::cout << "=== Toguz Korgool Game Manager with Fixed TUZ Logic ===\n";
    std::cout << "Movement: Counter-clockwise (against the clock)\n";
    std::cout << "Top side: moves left to right (0→1→2→...→8)\n";
    std::cout << "Down side: moves right to left (8→7→6→...→0)\n";
    std::cout << "TUZ Logic: -1 = no tuz, 0-8 = tuz position\n\n";
    
    // Установка начального ID
    game_manager.set_last_game_id_from_db(100);
    
    // Создаем и запускаем несколько автоматических игр
    game_manager.create_and_play_multiple_games(100, true);
    
    std::cout << "\n=== Creating more games for statistics ===\n";
    
    // Создаем еще больше игр для демонстрации
    game_manager.create_and_play_multiple_games(50, false);
    
    // Показываем финальную статистику
    std::cout << "\n=== Final Statistics ===\n";
    game_manager.show_games_stats();
    
    // Демонстрация поиска игр по статусу
    std::cout << "\n=== Games by Status ===\n";
    auto finished_games = game_manager.get_games_by_status(GameStatus::PLAYER1_WINS);
    std::cout << "Player 1 wins: " << finished_games.size() << "\n";
    
    finished_games = game_manager.get_games_by_status(GameStatus::PLAYER2_WINS);
    std::cout << "Player 2 wins: " << finished_games.size() << "\n";
    
    finished_games = game_manager.get_games_by_status(GameStatus::DRAW);
    std::cout << "Draws: " << finished_games.size() << "\n";
    
    game_manager.show_comprehensive_statistics();
    
    // Очищаем завершенные игры
    std::cout << "\n=== Removing finished games ===\n";
    game_manager.remove_finished_games();
    game_manager.show_games_stats();
    */

    return 0;
}