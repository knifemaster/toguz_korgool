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

