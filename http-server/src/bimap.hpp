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
