#include <iostream>
#include <coroutine>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cstring>

#include <sstream>
#include <chrono>
#include <ctime>
#include <cmath>

#include "match_maker.hpp"
#include "game_manager2.hpp"

#include "bimap.hpp"


constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 10;


std::atomic<int> game_id_count = 0;

int generate_game_id() {
    return game_id_count.fetch_add(1, std::memory_order_relaxed);
}

//std::unordered_map <int, Player>
// Утилита для установки non-blocking сокета
void set_nonblocking(int fd) {

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

