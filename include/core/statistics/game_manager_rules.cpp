#include <iostream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>

class Ring {
public:
    std::vector<int> top{9, 9, 9, 30, 9, 9, 9, 9, 9};  // Лунки первого игрока (индексы 0-8)
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

class ToguzKorgoolGame {
private:
    Ring board;
    mutable std::mutex board_mutex;
    bool game_over = false;
    int top_tuz = -1;     // Позиция туза первого игрока (-1 = нет туза)
    int down_tuz = -1;    // Позиция туза второго игрока (-1 = нет туза)
    bool top_tuz_used = false;   // Использовал ли первый игрок туз
    bool down_tuz_used = false;  // Использовал ли второй игрок туз

    // Структура для отслеживания последней позиции при распределении
    struct LastPosition {
        std::vector<int>* side;
        int position;
        bool is_opponent_side;
    };

    // Распределение коргоолов против часовой стрелки (ИСПРАВЛЕНО)
    LastPosition distribute_korgools(int korgools, int start_pos, bool is_top_player) {
        int current_pos = start_pos;
        bool current_is_top = is_top_player;
        std::vector<int>* last_side = nullptr;
        int last_pos = -1;

        for (int i = 0; i < korgools; ++i) {
            // Движение против часовой стрелки
            if (current_is_top) {
                // На верхней стороне движемся справа налево
                if (current_pos == 0) {
                    // Переход на нижнюю сторону, начинаем с позиции 0
                    current_is_top = false;
                    current_pos = 0;
                } else {
                    current_pos--;  // Движение справа налево (8→7→6→...→0)
                }
            } else {
                // На нижней стороне движемся слева направо
                if (current_pos == 8) {
                    // Переход на верхнюю сторону, начинаем с позиции 8
                    current_is_top = true;
                    current_pos = 8;
                } else {
                    current_pos++;  // Движение слева направо (0→1→2→...→8)
                }
            }

            // Добавляем коргоол в текущую позицию
            std::vector<int>& target_side = current_is_top ? board.top : board.down;
            target_side[current_pos]++;
            
            std::cout << "  Add to " << (current_is_top ? "top" : "down") 
                      << "[" << current_pos << "] = " << target_side[current_pos] << "\n";
            
            last_side = &target_side;
            last_pos = current_pos;
        }

        return {last_side, last_pos, 
                (is_top_player && !current_is_top) || (!is_top_player && current_is_top)};
    }

    // Проверка и обработка захвата коргоолов
    void check_capture(const LastPosition& last_pos, bool is_top_player) {
        // Захват происходит только если последний коргоол попал на сторону противника
        if (!last_pos.is_opponent_side) return;
        
        int korgools_in_hole = (*last_pos.side)[last_pos.position];
        
        // Захват при четном количестве коргоолов (и не пустой лунке)
        if (korgools_in_hole % 2 == 0 && korgools_in_hole > 0) {
            std::cout << "  Capture " << korgools_in_hole << " korgools from " 
                      << (last_pos.side == &board.top ? "top" : "down") 
                      << "[" << last_pos.position << "]\n";
            
            // Забираем коргоолы в казан
            if (is_top_player) {
                board.top_kazan += korgools_in_hole;
            } else {
                board.down_kazan += korgools_in_hole;
            }
            (*last_pos.side)[last_pos.position] = 0;
        }
    }

    // Проверка и обработка туза
    void check_tuz(const LastPosition& last_pos, bool is_top_player) {
        // Туз объявляется только если последний коргоол попал на сторону противника
        if (!last_pos.is_opponent_side) return;
        
        int korgools_in_hole = (*last_pos.side)[last_pos.position];
        
        // Условия для объявления туза:
        // 1. В лунке стало ровно 3 коргоола
        // 2. Это не 9-я лунка (индекс 8)
        // 3. Игрок еще не использовал туз
        // 4. Противник не объявил эту лунку тузом
        if (korgools_in_hole == 3 && last_pos.position != 8) {
            bool& player_tuz_used = is_top_player ? top_tuz_used : down_tuz_used;
            int& player_tuz = is_top_player ? top_tuz : down_tuz;
            int& opponent_tuz = is_top_player ? down_tuz : top_tuz;
            
            if (!player_tuz_used && opponent_tuz != last_pos.position) {
                // Объявляем туз
                player_tuz = last_pos.position;
                player_tuz_used = true;
                
                std::cout << "  Player " << (is_top_player ? "1" : "2") 
                          << " declares TUZ at " << (last_pos.side == &board.top ? "top" : "down") 
                          << "[" << last_pos.position << "]\n";
                
                // Забираем коргоолы из тузовой лунки
                if (is_top_player) {
                    board.top_kazan += korgools_in_hole;
                } else {
                    board.down_kazan += korgools_in_hole;
                }
                (*last_pos.side)[last_pos.position] = 0;
            }
        }
    }

    // Проверка тузовых лунок после каждого хода
    void check_tuz_holes() {
        // Проверяем туз первого игрока
        if (top_tuz_used && top_tuz != -1) {
            std::vector<int>& tuz_side = (top_tuz < 9) ? board.down : board.top;
            int tuz_pos = top_tuz;
            
            if (tuz_side[tuz_pos] > 0) {
                std::cout << "  Player 1 captures " << tuz_side[tuz_pos] 
                          << " korgools from TUZ hole [" << tuz_pos << "]\n";
                board.top_kazan += tuz_side[tuz_pos];
                tuz_side[tuz_pos] = 0;
            }
        }
        
        // Проверяем туз второго игрока
        if (down_tuz_used && down_tuz != -1) {
            std::vector<int>& tuz_side = (down_tuz < 9) ? board.top : board.down;
            int tuz_pos = down_tuz;
            
            if (tuz_side[tuz_pos] > 0) {
                std::cout << "  Player 2 captures " << tuz_side[tuz_pos] 
                          << " korgools from TUZ hole [" << tuz_pos << "]\n";
                board.down_kazan += tuz_side[tuz_pos];
                tuz_side[tuz_pos] = 0;
            }
        }
    }

    // Проверка окончания игры
    bool check_game_end() {
        // Победа по количеству коргоолов
        if (board.top_kazan >= WIN_SCORE) {
            std::cout << "Player 1 wins with " << board.top_kazan << " korgools!\n";
            return true;
        }
        if (board.down_kazan >= WIN_SCORE) {
            std::cout << "Player 2 wins with " << board.down_kazan << " korgools!\n";
            return true;
        }
        
        // Проверка на отсутствие коргоолов у игрока
        bool top_has_korgools = false;
        bool down_has_korgools = false;
        
        for (int i = 0; i < 9; ++i) {
            if (board.top[i] > 0) top_has_korgools = true;
            if (board.down[i] > 0) down_has_korgools = true;
        }
        
        if (!top_has_korgools || !down_has_korgools) {
            std::cout << "Game ends - one player has no korgools left!\n";
            if (board.top_kazan > board.down_kazan) {
                std::cout << "Player 1 wins!\n";
            } else if (board.down_kazan > board.top_kazan) {
                std::cout << "Player 2 wins!\n";
            } else {
                std::cout << "Draw!\n";
            }
            return true;
        }
        
        // Проверка на ничью (по 81 коргоолу)
        if (board.top_kazan == 81 && board.down_kazan == 81) {
            std::cout << "Draw! Both players have 81 korgools.\n";
            return true;
        }
        
        return false;
    }

public:
    static constexpr int WIN_SCORE = 82;

    bool move(int move_number, int position, bool is_top_player) {
        std::lock_guard<std::mutex> lock(board_mutex);
        if (game_over) return false;

        auto& current_side = is_top_player ? board.top : board.down;
        
        // Проверка корректности хода
        if (position < 0 || position >= current_side.size() || current_side[position] == 0) {
            std::cout << "Invalid move!\n";
            return false;
        }

        int korgools = current_side[position];
        current_side[position] = 0;

        std::cout << "\n=== Move " << move_number << " ===\n";
        std::cout << "Player " << (is_top_player ? "1 (top)" : "2 (down)") 
                  << " moves from hole " << position << " with " << korgools << " korgools\n";

        // Распределение коргоолов
        LastPosition last_pos = distribute_korgools(korgools, position, is_top_player);

        // Проверка и обработка захвата
        check_capture(last_pos, is_top_player);

        // Проверка и обработка туза
        check_tuz(last_pos, is_top_player);

        // Проверка тузовых лунок
        check_tuz_holes();

        // Проверка окончания игры
        game_over = check_game_end();

        board.print();
        return true;
    }

    void print_board() const {
        std::lock_guard<std::mutex> lock(board_mutex);
        board.print();
        
        // Вывод информации о тузах
        if (top_tuz_used) {
            std::cout << "Player 1 TUZ: " << (top_tuz < 9 ? "down" : "top") 
                      << "[" << top_tuz << "]\n";
        }
        if (down_tuz_used) {
            std::cout << "Player 2 TUZ: " << (down_tuz < 9 ? "top" : "down") 
                      << "[" << down_tuz << "]\n";
        }
    }

    bool is_game_over() const {
        std::lock_guard<std::mutex> lock(board_mutex);
        return game_over;
    }

    std::pair<int, int> get_scores() const {
        std::lock_guard<std::mutex> lock(board_mutex);
        return {board.top_kazan, board.down_kazan};
    }
};

int main() {
    ToguzKorgoolGame game;
    
    std::cout << "=== Toguz Korgool Game ===\n";
    std::cout << "Movement: Counter-clockwise (against the clock)\n";
    std::cout << "Top side: moves right to left (8→7→6→...→0)\n";
    std::cout << "Down side: moves left to right (0→1→2→...→8)\n";
    std::cout << "Initial state:\n";
    game.print_board();
    
    // Пример игры
    std::vector<std::pair<int, bool>> moves = {
        {3, true},   // Игрок 1 ходит из лунки 3
        {1, false},  // Игрок 2 ходит из лунки 1
        {5, true},   // Игрок 1 ходит из лунки 5
        {4, false},  // Игрок 2 ходит из лунки 4
        {0, true},   // Игрок 1 ходит из лунки 0
        {2, false}   // Игрок 2 ходит из лунки 2
    };
    
    for (size_t i = 0; i < moves.size() && !game.is_game_over(); ++i) {
        if (!game.move(i + 1, moves[i].first, moves[i].second)) {
            std::cout << "Move failed!\n";
            break;
        }
        
        auto scores = game.get_scores();
        std::cout << "Scores: Player 1: " << scores.first 
                  << ", Player 2: " << scores.second << "\n";
        
        // Пауза для читабельности
        std::cout << "Press Enter to continue...\n";
        std::cin.get();
    }
    
    return 0;
}