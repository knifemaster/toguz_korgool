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


constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 10;

// Утилита для установки non-blocking сокета
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Awaitable для событий epoll
struct EpollAwaiter {
    int epoll_fd;
    int fd;
    uint32_t events;
    std::coroutine_handle<> handle;

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        handle = h;
        epoll_event ev{};
        ev.events = events;
        ev.data.ptr = this;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    }

    void await_resume() const noexcept {}
};


struct Task {
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
};
