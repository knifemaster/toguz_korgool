#include <iostream>
#include <string>
#include <chrono>
#include <ctime>
#include <vector>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <coroutine>
#include <queue>
#include <unordered_map>
#include <stdexcept>
#include <expected>

static int epoll_fd = -1;

// Awaitable объект для операций ввода-вывода
struct io_operation {
    int fd;
    uint32_t events;
    
    bool await_ready() const noexcept { return false; }
    
    void await_suspend(std::coroutine_handle<> h) const {
        epoll_event ev{};
        ev.events = events;
        ev.data.ptr = h.address();
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
          throw std::runtime_error("epoll_ctl failed");
        }
    }
    
    void await_resume() const {
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
          throw std::runtime_error("epoll_ctl delete failed");
        }
    }
};

struct Task {
    struct promise_type {
        Task get_return_object() { return Task{}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }

        // Разрешаем co_await только для io_operation
        io_operation await_transform(io_operation op) { return op; }
    };
};

io_operation async_read(int fd) {
    return {fd, EPOLLIN};
}

io_operation async_write(int fd) {
    return {fd, EPOLLOUT};
}

std::expected<std::string, std::string> read_line(int fd) {
    char buf[1024];
    std::string buffer;

    while (true) {
        async_read(fd);

        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) {
            if (n == 0) {
              return std::unexpected("Client disconnected");
            } else {
              return std::unexpected(std::string(strerror(errno)));
            }
        }

        buffer.append(buf, n);
        if (buffer.find('\n') != std::string::npos) {
            buffer.erase(buffer.find_last_not_of("\n\r") + 1);
            break;
        }
    }
    return buffer;
}

