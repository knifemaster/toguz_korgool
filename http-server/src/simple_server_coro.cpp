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

