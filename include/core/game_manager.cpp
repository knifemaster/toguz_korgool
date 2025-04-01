#include <iostream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <thread>

class Board {
public:
    static constexpr size_t DEFAULT_CELL_COUNT = 9;
    
    Board(int id) : id(id) {
        player1.resize(DEFAULT_CELL_COUNT, 9);
        player2.resize(DEFAULT_CELL_COUNT, 9);
        player1[5] = 20;
    }


    const std::vector<int>& get_player1_board() const { return player1; }
    const std::vector<int>& get_player2_board() const { return player2; }

    void print_boards() const {
        std::cout << "Player 1: ";
        for (const auto& cell : player1) std::cout << cell << " ";
        std::cout << "\nPlayer 2: ";
        for (const auto& cell : player2) std::cout << cell << " ";
        std::cout << "\n";
    }


    void move(int position, bool color) {
        std::lock_guard<std::mutex> lock(board_mutex);
        if (color) { // white
            if (position < 0 || position >= static_cast<int>(player1.size())) return;
            
            int count = player1[player1.size() - 1 - position];
            player1[player1.size() - 1 - position] = 0;
            
            for (int i = player1.size() - 1 - position; i >= 0; --i) {
                player1[i]++;
            }
        } else {
            // TODO: Implement black player move
        }
    }

private:
    int id;
    std::vector<int> player1;
    std::vector<int> player2;
    mutable std::mutex board_mutex;
};


num class GameStatus { ACTIVE, FINISHED };

class Game {
public:
    Game(int game_id, GameStatus status)
        : id(game_id), status(status), board(std::make_unique<Board>(game_id)) {}

    int get_id() const { return id; }
    GameStatus get_status() const { return status; }
    Board* get_board() { return board.get(); }
    const Board* get_board() const { return board.get(); }

    void move(int position, bool color) {
        board->move(position, color);
    }

private:
    int id;
    GameStatus status;
    std::unique_ptr<Board> board;
};


class GameManager {
public:
    void create_game(int game_id) {
        std::unique_lock<std::shared_mutex> lock(games_mutex_);
        if (games_.find(game_id) != games_.end()) {
            throw std::runtime_error("Game with this ID already exists");
        }
        games_.emplace(game_id, std::make_unique<Game>(game_id, GameStatus::ACTIVE));
    }

    Game* get_game(int game_id) {
        std::shared_lock<std::shared_mutex> lock(games_mutex_);
        auto it = games_.find(game_id);
        if (it == games_.end()) {
            throw std::runtime_error("Game not found");
        }
        return it->second.get();
    }

    void make_move(int game_id, int position, bool color) {
        Game* game = get_game(game_id);
        game->move(position, color);
    }

    void print_all_games() const {
        std::shared_lock<std::shared_mutex> lock(games_mutex_);
        for (const auto& [id, game] : games_) {
            std::cout << "Game ID: " << id << "\n";
            game->get_board()->print_boards();
        }
    }

private:
    mutable std::shared_mutex games_mutex_;
    std::unordered_map<int, std::unique_ptr<Game>> games_;
};



void player_thread(GameManager& manager, int game_id, int moves) {
    try {
        for (int i = 0; i < moves; ++i) {
            manager.make_move(game_id, i % 9, true); // Чередуем позиции
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } catch (const std::exception& e) {
        std::cerr << "Thread error: " << e.what() << "\n";
    }
}
