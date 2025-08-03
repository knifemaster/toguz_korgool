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
    bool top_tuz_used = false;   // Использовал ли первый игрок туз
    bool down_tuz_used = false;  // Использовал ли второй игрок туз
    int game_id;         // ID игры (теперь int)
    int move_count = 0;          // Счетчик ходов

    // Структура для отслеживания последней позиции при распределении
    //
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

            // Добавляем коргоол в текущую позицию
            // добавить if ()
            std::vector<int>& target_side = current_is_top ? board->top : board->down;
            
            // здесь надо внимательнее
            // переделать
            
            //check_capture(current_pos, is_top_player);
            // check_tuz_holes();
            // пока так
            if (top_tuz == current_pos) {
                board->down_kazan++;
                last_side = &target_side;
                last_pos = current_pos;
                continue;
            } 
            if (down_tuz == current_pos) {
                board->top_kazan++;
                last_side = &target_side;
                last_pos = current_pos;
                continue;
            }


            target_side[current_pos]++;
            
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
            // Забираем коргоолы в казан
            if (is_top_player) {
                board->top_kazan += korgools_in_hole;
            } else {
                board->down_kazan += korgools_in_hole;
            }
            (*last_pos.side)[last_pos.position] = 0;
        }
    }

/*
    void capture_by_tuz(int current_position, bool is_top_player) {
   
    }
 */
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
            std::cout << "@@@@@@@@@@@@make tuz" << std::endl;
            std::cout << "position: "<< last_pos.position << std::endl;
            std::cout << "is_top_player" << is_top_player << std::endl;
            std::cout << "top_tuz" << top_tuz << std::endl;
            std::cout << "down_tuz" << down_tuz << std::endl;
            bool& player_tuz_used = is_top_player ? top_tuz_used : down_tuz_used;
            int& player_tuz = is_top_player ? top_tuz : down_tuz;
            int& opponent_tuz = is_top_player ? down_tuz : top_tuz;
            
            if (!player_tuz_used && opponent_tuz != last_pos.position) {
                // Объявляем туз
                player_tuz = last_pos.position;
                

            // здесь повнимательней
            /*
                if (is_top_player && top_tuz == -1) {    
                        top_tuz = last_pos.position;
                        std::cout << "((((((((*((*(*(*(*(*(*(*(*(*(*(*()))))))))))))))))))))" << top_tuz << std::endl;
                }   else if (!is_top_player && down_tuz == -1) {
                        down_tuz = last_pos.position;
                        std::cout << "*****************************************************" << down_tuz << std::endl;
                }
            */
                player_tuz_used = true;
                
                // Забираем коргоолы из тузовой лунки
                if (is_top_player) {
                    board->top_kazan += korgools_in_hole;
                } else {
                    board->down_kazan += korgools_in_hole;
                }
                (*last_pos.side)[last_pos.position] = 0;
            }
        }
    }

    // Проверка тузовых лунок после каждого хода
    // проверка тузов как определяем позицию туза
    // и где 
    void check_tuz_holes() {
        // Проверяем туз первого игрока

        /*
            if (top_tuz_used && top_tuz != -1) {
            std::vector<int>& tuz_side = (top_tuz < 9) ? board->down : board->top;
            int tuz_pos = top_tuz;
            
            if (tuz_side[tuz_pos] > 0) {
                std::cout << "__________________top tuz" << std::endl;
                board->top_kazan += tuz_side[tuz_pos];
                tuz_side[tuz_pos] = 0;
            }
        }
        
        // Проверяем туз второго игрока
        if (down_tuz_used && down_tuz != -1) {
            std::vector<int>& tuz_side = (down_tuz < 9) ? board->top : board->down;
            int tuz_pos = down_tuz;
            
            if (tuz_side[tuz_pos] > 0) {
                std::cout << "__________________top tuz" << std::endl;
                board->down_kazan += tuz_side[tuz_pos];
                tuz_side[tuz_pos] = 0;
            }
        }
        */
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

    // Создать новую игру

    void set_last_game_id_from_db(int id) {
        next_id = id;
    }

    int create_game() {
        std::lock_guard<std::mutex> lock(games_mutex);
        
       

        int game_id = generate_game_id();
        games[game_id] = std::make_unique<ToguzKorgoolGame>(game_id);
        
        std::cout << "Created new game with ID: " << game_id << "\n";
        return game_id;
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

    // Получить количество активных игр
    size_t get_games_count() const {
        std::lock_guard<std::mutex> lock(games_mutex);
        return games.size();
    }

    // Очистить все игры
    void clear_all_games() {
        std::lock_guard<std::mutex> lock(games_mutex);
        games.clear();
        std::cout << "All games cleared.\n";
    }

    // Удалить завершенные игры
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

    // Получить игры по статусу
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

    // Показать статистику всех игр
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

    // Автоматическая игра со случайными ходами
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
        std::uniform_int_distribution<> delay_dist(50, 200);

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

    // Создать и запустить несколько автоматических игр
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

int main() {
    GameManager game_manager;
    
    std::cout << "=== Toguz Korgool Game Manager with Integer IDs ===\n";
    std::cout << "Movement: Counter-clockwise (against the clock)\n";
    std::cout << "Top side: moves left to right (0→1→2→...→8)\n";
    std::cout << "Down side: moves right to left (8→7→6→...→0)\n\n";
    
    game_manager.set_last_game_id_from_db(100);
    // Создаем и запускаем несколько автоматических игр
    game_manager.create_and_play_multiple_games(5, true);
    
    std::cout << "\n=== Creating more games for statistics ===\n";
    
    // Создаем еще больше игр для демонстрации
    game_manager.create_and_play_multiple_games(10, false);
    
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
    
    // Очищаем завершенные игры
    std::cout << "\n=== Removing finished games ===\n";
    game_manager.remove_finished_games();
    game_manager.show_games_stats();
    
    return 0;
}