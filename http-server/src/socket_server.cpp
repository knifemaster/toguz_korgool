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


std::mutex connections_mutex;
std::unordered_map<int, Connection> connections;

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
    }
}


int create_listen_socket(int port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listen_fd == -1) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(listen_fd);
        return -1;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr))) {
        perror("bind");
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, SOMAXCONN)) {
        perror("listen");
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}


std::string process_command(const std::string& cmd) {
    std::stringstream ss(cmd);
    std::string command;
    ss >> command;
    std::transform(command.begin(), command.end(), command.begin(), ::tolower);

    if (command == "hello") {
        return "Hello! How can I help you?\n";
    }
    else if (command == "time") {
        time_t now = time(nullptr);
        return std::string("Server time: ") + ctime(&now);
    }
    else if (command == "stats") {
        std::lock_guard<std::mutex> lock(connections_mutex);
        return "Active connections: " + std::to_string(connections.size()) + "\n";
    }
    else if (command == "echo") {
        std::string rest;
        std::getline(ss, rest);
        if (!rest.empty() && rest[0] == ' ') {
            rest.erase(0, 1);
        }
        return "ECHO: " + rest + "\n";
    }
    else if (command == "quit") {
        return "Goodbye!\n";
    }
    else {
        return "Unknown command. Available commands: HELLO, TIME, STATS, ECHO, QUIT\n";
    }
}
