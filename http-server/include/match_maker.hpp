#include <iostream>
#include <set>
#include <chrono>
#include <random>
#include <vector>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>



struct Player {
    int id;
    int rating;
    std::chrono::time_point<std::chrono::system_clock> joinTime;
};

