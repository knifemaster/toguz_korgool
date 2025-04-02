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


// передаем  через параметры
// состояния игр id_игры
//
void process_client_data(int fd) {
    std::lock_guard<std::mutex> lock(connections_mutex);
    if (connections.find(fd) == connections.end()) return;

    auto& conn = connections[fd];
    conn.last_activity = time(nullptr);

    size_t pos;
    while ((pos = conn.buffer.find('\n')) != std::string::npos) {
        std::string cmd = conn.buffer.substr(0, pos);
        conn.buffer.erase(0, pos + 1);

        cmd.erase(std::remove(cmd.begin(), cmd.end(), '\r'), cmd.end());

        if (!cmd.empty()) {
            std::cout << "Processing command from fd " << fd << ": " << cmd << std::endl;

            // логика игры здесь
            // запускаем игры
            std::string response = process_command(cmd);
            // генерируем ответ
            ssize_t sent = send(fd, response.c_str(), response.size(), 0);
            if (sent == -1) {
                perror("send");
                close(fd);
                connections.erase(fd);
                return;
            }

            if (cmd.find("quit") == 0) {
                close(fd);
                connections.erase(fd);
                return;
            }
        }
    }

    if (conn.buffer.size() > MAX_BUFFER_SIZE) {
        std::cerr << "Buffer overflow for fd " << fd << ", closing connection" << std::endl;
        send(fd, "ERROR: Buffer overflow, connection closed\n", 41, 0);
        close(fd);
        connections.erase(fd);
    }
}


void worker_thread(int epoll_fd) {
    epoll_event events[MAX_EVENTS];

    while (!stop_flag) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, 100);
        if (num_events == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < num_events; ++i) {
            int fd = events[i].data.fd;

            if (events[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP)) {
                if (connections.find(fd) != connections.end()) {
                    char buf[MAX_CMD_LENGTH];
                    while (true) {
                        ssize_t count = read(fd, buf, sizeof(buf));
                        if (count == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                process_client_data(fd);
                                break;
                            } else {
                                perror("read");
                                std::lock_guard<std::mutex> lock(connections_mutex);
                                close(fd);
                                connections.erase(fd);
                                break;
                            }
                        } else if (count == 0) {
                            std::lock_guard<std::mutex> lock(connections_mutex);
                            close(fd);
                            connections.erase(fd);
                            break;
                        } else {
                            {
                                std::lock_guard<std::mutex> lock(connections_mutex);
                                connections[fd].buffer.append(buf, count);
                            }
                        }
                    }
                }
            }
        }
    }
}


void check_timeouts() {
    while (!stop_flag) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        time_t now = time(nullptr);
        std::lock_guard<std::mutex> lock(connections_mutex);

        for (auto it = connections.begin(); it != connections.end(); ) {
            if (now - it->second.last_activity > 60) {
                std::cout << "Connection timeout for fd " << it->first << std::endl;
                close(it->first);
                it = connections.erase(it);
            } else {
                ++it;
            }
        }
    }
}


int main() {
    std::vector<int> listen_sockets;
    for (int port = 8080; port < 8085; ++port) {
        int sock = create_listen_socket(port);
        if (sock != -1) {
            listen_sockets.push_back(sock);
            std::cout << "Listening on port " << port << std::endl;
        }
    }

    if (listen_sockets.empty()) {
        std::cerr << "No listening sockets created" << std::endl;
        return 1;
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        return 1;
    }

    for (int sock : listen_sockets) {
        epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = sock;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &event)) {
            perror("epoll_ctl");
            close(sock);
        }
    }

    std::vector<std::thread> workers;
    for (int i = 0; i < NUM_WORKERS; ++i) {
        workers.emplace_back(worker_thread, epoll_fd);
    }

    std::thread timeout_thread(check_timeouts);

    while (!stop_flag) {
        epoll_event events[MAX_EVENTS];
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, 100);
        if (num_events == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < num_events; ++i) {
            int fd = events[i].data.fd;

            if (std::find(listen_sockets.begin(), listen_sockets.end(), fd) != listen_sockets.end()) {
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(fd, (sockaddr*)&client_addr, &client_len);
                if (client_fd == -1) {
                    perror("accept");
                    continue;
                }

                set_nonblocking(client_fd);

                epoll_event event;
                event.events = EPOLLIN | EPOLLET;
                event.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event)) {
                    perror("epoll_ctl");
                    close(client_fd);
                    continue;
                }

                std::lock_guard<std::mutex> lock(connections_mutex);
                connections[client_fd] = {client_fd, "", time(nullptr)};

                std::string welcome = "Welcome to the server! Available commands:\n"
                                    "HELLO - Greeting\n"
                                    "TIME - Server time\n"
                                    "STATS - Connection statistics\n"
                                    "ECHO <text> - Echo text\n"
                                    "QUIT - Disconnect\n";
                send(client_fd, welcome.c_str(), welcome.size(), 0);
            }
        }
    }

    stop_flag = true;
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    timeout_thread.join();

    std::lock_guard<std::mutex> lock(connections_mutex);
    for (const auto& conn : connections) {
        close(conn.first);
    }
    connections.clear();

    for (int sock : listen_sockets) {
        close(sock);
    }

    close(epoll_fd);

    return 0;
}

