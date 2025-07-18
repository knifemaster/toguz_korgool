#include <iostream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <string>
#include <random>
#include <chrono>
#include <thread>

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
                if (down_tuz != -1 && down_tuz == current_pos) {
                    std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!коргоол в казане" << std::endl;
                    // Это туз второго игрока - коргоол идет во второй казан
                    board->down_kazan++;
                    captured_by_tuz = true;
                } else if (top_tuz != -1 && top_tuz == current_pos) {
                    std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!коргоол в казане" << std::endl;
                    // Это туз первого игрока - коргоол идет в первый казан
                    board->top_kazan++;
                    captured_by_tuz = true;
                }
            } else {
                // Находимся на нижней стороне (down)
                if (top_tuz != -1 && top_tuz == current_pos) {
                    std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!коргоол в казане" << std::endl;
                    // Это туз первого игрока - коргоол идет в первый казан
                    board->top_kazan++;
                    captured_by_tuz = true;
                } else if (down_tuz != -1 && down_tuz == current_pos) {
                    std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!коргоол в казане" << std::endl;
                    // Это туз второго игрока - коргоол идет во второй казан
                    board->down_kazan++;
                    captured_by_tuz = true;
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
                //target_side[current_pos]++;
                last_side = &target_side;
                last_pos = current_pos;


            }
            // Если коргоол захвачен тузом, last_side и last_pos остаются от предыдущего коргоола
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
            return true;
        }
        if (board->down_kazan >= WIN_SCORE) {
            status = GameStatus::PLAYER2_WINS;
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
            return true;
        }
        
        // Проверка на ничью (по 81 коргоолу)
        if (board->top_kazan == 81 && board->down_kazan == 81) {
            status = GameStatus::DRAW;
            return true;
        }
        
        return false;
    }

public:
    static constexpr int WIN_SCORE = 82;

    // Конструктор с ID игры
    ToguzKorgoolGame(int id = 0) : game_id(id) {
        board = std::make_shared<Ring>();
    }

    // Начать игру (переводит из состояния ожидания в активное состояние)
    void start_game() {
        std::lock_guard<std::mutex> lock(board_mutex);
        if (status == GameStatus::WAITING_FOR_PLAYERS) {
            status = GameStatus::IN_PROGRESS;
        }
    }

    // Прервать игру
    void abandon_game() {
        std::lock_guard<std::mutex> lock(board_mutex);
        if (status == GameStatus::IN_PROGRESS || status == GameStatus::WAITING_FOR_PLAYERS) {
            status = GameStatus::ABANDONED;
            game_over = true;
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

        // Распределение коргоолов с учетом тузов
        LastPosition last_pos = distribute_korgools(korgools, position, is_top_player);

        // Проверка и обработка захвата (только если последний коргоол не был захвачен тузом)
        if (last_pos.side != nullptr) {
            check_capture(last_pos, is_top_player);
            
            // Проверка и обработка туза (только если последний коргоол не был захвачен тузом)
            check_tuz(last_pos, is_top_player);
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

    int get_game_id() const {
        return game_id;
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

