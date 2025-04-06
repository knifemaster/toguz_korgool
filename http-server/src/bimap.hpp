#include <unordered_map>
#include <string>
#include <stdexcept>
#include <iostream>
#include <mutex>

template <typename Left, typename Right>
class Bimap {
    std::unordered_map<Left, Right> left_to_right;
    std::unordered_map<Right, Left> right_to_left;
    std::mutex bimap_mutex;

public:
    void insert(const Left& l, const Right& r) {
        std::lock_guard<std::mutex> lock(bimap_mutex);
        left_to_right[l] = r;
        right_to_left[r] = l;
    }
