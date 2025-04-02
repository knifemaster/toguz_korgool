#include <iostream>
#include <vector>
#include <thread>
#include <unordered_map>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <atomic>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <ctime>


constexpr int MAX_EVENTS = 64;
constexpr int NUM_WORKERS = 4;
constexpr int MAX_CMD_LENGTH = 1024;
constexpr int MAX_BUFFER_SIZE = 8192;
std::atomic<bool> stop_flag{false};

struct Connection {
    int fd;
    std::string buffer;
    time_t last_activity;
};
