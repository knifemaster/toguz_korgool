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
