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

