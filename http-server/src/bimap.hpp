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

    Right at_left(const Left& l) const {

        return left_to_right.at(l);
    }

    Left at_right(const Right& r) const {
        return right_to_left.at(r);
    }
};


int main() {
    Bimap<int, int> socket_descriptors;
    socket_descriptors.insert(43123, 3213);
    socket_descriptors.insert(4324, 43241);

    std::cout << socket_descriptors.at_left(4324) << '\n';  // 1
    std::cout << socket_descriptors.at_right(3213) << '\n';     // "two"
}
