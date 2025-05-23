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
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <stdexcept>
#include <memory>
#include <thread>
#include <vector>
#include <functional>

#include "match_maker.hpp"
#include "game_manager2.hpp"
#include "bimap.hpp"

constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 10;

std::atomic<int> game_id_count = 0;

int generate_game_id() {
    return game_id_count.fetch_add(1, std::memory_order_relaxed);
}

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("Failed to set non-blocking mode");
    }
}

class SocketGuard {
    int fd_ = -1;
public:
    explicit SocketGuard(int fd = -1) : fd_(fd) {}
    ~SocketGuard() { if (fd_ != -1) close(fd_); }

    SocketGuard(SocketGuard&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    SocketGuard& operator=(SocketGuard&& other) noexcept {
        if (this != &other) {
            if (fd_ != -1) close(fd_);
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    operator int() const { return fd_; }
    int release() { int fd = fd_; fd_ = -1; return fd; }
};

class ThreadPool {
public:
    explicit ThreadPool(size_t n) : stop(false) {
        for (size_t i = 0; i < n; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(mtx);
                        cv.wait(lock, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    try { task(); }
                    catch (const std::exception& e) {
                        std::cerr << "Task error: " << e.what() << std::endl;
                    }
                }
            });
        }
    }

    template <class F>
    void enqueue(F&& f) {
        {
            std::lock_guard lock(mtx);
            if (stop) throw std::runtime_error("Enqueue on stopped pool");
            tasks.emplace(std::forward<F>(f));
        }
        cv.notify_one();
    }

    ~ThreadPool() {
        {
            std::lock_guard lock(mtx);
            stop = true;
        }
        cv.notify_all();
        for (auto& t : workers) if (t.joinable()) t.join();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop;
};

struct Task {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    handle_type coro;
    Task(handle_type h) : coro(h) {}
    void start() { coro.resume(); }

    struct promise_type {
        Task get_return_object() { return Task{handle_type::from_promise(*this)}; }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {
            std::cerr << "Coroutine exception" << std::endl;
        }
    };
};

struct EpollAwaiter {
    int epoll_fd;
    int fd;
    uint32_t events;
    std::coroutine_handle<> handle;

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        handle = h;
        epoll_event ev{};
        ev.events = events | EPOLLET;
        ev.data.ptr = this;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
            if (errno == ENOENT) {
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
                    perror("epoll_ctl ADD");
                    std::terminate();
                }
            } else {
                perror("epoll_ctl MOD");
                std::terminate();
            }
        }
    }

    void await_resume() const noexcept {}
};

Task client_coroutine(int epoll_fd, int client_fd, GameManager& manager, 
                      Bimap<int, int>& sockets_descriptors, 
                      ThreadSafeMatchmaker& matchmaker) {
    char buffer[1024];
    SocketGuard client_guard(client_fd);

    try {
        while (true) {
            co_await EpollAwaiter{epoll_fd, client_fd, EPOLLIN};

            while (true) {
                int valread = read(client_fd, buffer, sizeof(buffer) - 1);
                if (valread == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                    throw std::runtime_error("read error");
                }
                if (valread == 0) throw std::runtime_error("client disconnected");

                buffer[valread] = '\0';
                std::string input(buffer);
                std::istringstream iss(input);
                std::string command;
                iss >> command;
                std::string response;

                if (command == "find_game") {
                    int id, rating;
                    if (iss >> id >> rating) {
                        matchmaker.addPlayer({client_fd, id, rating, std::chrono::system_clock::now()});
                        std::pair<Player, Player> match;
                        if (matchmaker.tryFindMatch(match)) {

                            sockets_descriptors.insert(match.second.client_fd, match.first.client_fd);
                            
                            int game_id = generate_game_id();
                            std::cout << "game_id " << game_id;
                            manager.create_game(game_id);
                            std::cout << "fds :" << match.first.client_fd << " " << match.second.client_fd << std::endl;
                            send(sockets_descriptors.at_left(client_fd), "Hello", 5, 0);
                            send(match.first.client_fd, "white", 5, 0);
                            send(match.second.client_fd, "black", 5, 0);
                            continue;
                        }
                        response = "in queue";
                    }
                } 
                
                if (command == "move") {
                    int game_id; 
                    int position;
                    bool color; 
                    if (iss >> game_id >> position >> color) {
                        manager.make_move(game_id, position, color);
                    } 
                }

                else if (command == "echo") {
                    std::string rest;
                    std::getline(iss, rest);
                    response = "Echo: " + rest + "\n";
                } else if (command == "time") {
                    auto now = std::chrono::system_clock::now();
                    std::time_t t = std::chrono::system_clock::to_time_t(now);
                    response = "Time: " + std::string(std::ctime(&t));
                } else if (command == "pow") {
                    double base, exp;
                    if (iss >> base >> exp) {
                        response = "Result: " + std::to_string(std::pow(base, exp)) + "\n";
                    } else {
                        response = "Usage: pow <base> <exponent>\n";
                    }
                }

                if (!response.empty()) {
                    if (send(client_fd, response.c_str(), response.size(), 0) == -1) {
                        throw std::runtime_error("send failed");
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Client " << client_fd << " error: " << e.what() << std::endl;
    }
    co_return;
}

void init_socket_server() {
    GameManager manager;
    Bimap<int, int> socket_descriptors;
    ThreadSafeMatchmaker matchmaker;
    //ThreadPool pool(std::thread::hardware_concurrency());
    ThreadPool pool(8);
    
    SocketGuard server_guard(socket(AF_INET, SOCK_STREAM, 0));
    if (server_guard == -1) throw std::runtime_error("socket failed");

    int opt = 1;
    setsockopt(server_guard, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    set_nonblocking(server_guard);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_guard, (sockaddr*)&address, sizeof(address)) == -1) throw std::runtime_error("bind failed");
    if (listen(server_guard, SOMAXCONN) == -1) throw std::runtime_error("listen failed");

    SocketGuard epoll_guard(epoll_create1(0));
    if (epoll_guard == -1) throw std::runtime_error("epoll_create1 failed");

    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server_guard;
    epoll_ctl(epoll_guard, EPOLL_CTL_ADD, server_guard, &ev);

    epoll_event events[MAX_EVENTS];
    std::cout << "Server listening on port " << PORT << std::endl;

    while (true) {
        int n = epoll_wait(epoll_guard, events, MAX_EVENTS, -1);
        if (n == -1) {
            if (errno == EINTR) continue;
            throw std::runtime_error("epoll_wait failed");
        }

        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == server_guard) {
                while (true) {
                    int client_fd = accept(server_guard, nullptr, nullptr);
                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        throw std::runtime_error("accept failed");
                    }

                    try {
                        set_nonblocking(client_fd);
                        pool.enqueue([&manager, &matchmaker, &socket_descriptors, &epoll_guard, client_fd]() {
                        client_coroutine(epoll_guard, client_fd, manager, socket_descriptors, matchmaker).start();
                        });
                        //pool.enqueue([=, &manager, &matchmaker, &socket_descriptors]() {
                        //    client_coroutine(epoll_guard, client_fd, manager, socket_descriptors, matchmaker).start();
                        //});
                    } catch (...) {
                        close(client_fd);
                        throw;
                    }
                }
            } else {
                auto* awaiter = static_cast<EpollAwaiter*>(events[i].data.ptr);
                awaiter->handle.resume();
            }
        }
    }
}

int main() {
    try {
        init_socket_server();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
