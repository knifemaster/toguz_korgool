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

