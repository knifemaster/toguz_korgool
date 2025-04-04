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


// Обработка клиента с циклом сообщений
Task client_coroutine(int epoll_fd, int client_fd) {
    char buffer[1024] = {0};

    while (true) {
        // Ждём данные от клиента
        co_await EpollAwaiter{epoll_fd, client_fd, EPOLLIN};

        int valread = read(client_fd, buffer, sizeof(buffer) - 1);
        if (valread <= 0) {
            std::cout << "Client disconnected or read error.\n";
            close(client_fd);
            break;
        }

        buffer[valread] = '\0';
        std::string input(buffer);
        std::istringstream iss(input);
        std::string command;
        iss >> command;

        std::string response;

        if (command == "echo") {
            std::string rest;
            std::getline(iss, rest);
            response = "Echo: " + rest + "\n";

        } else if (command == "time") {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            response = "Time: " + std::string(std::ctime(&t));

        } else if (command == "message") {
            response = "Server says hello!\n";

        } else if (command == "pow") {
            double base, exp;
            if (iss >> base >> exp) {
                double result = std::pow(base, exp);
                response = "Result: " + std::to_string(result) + "\n";
            } else {
                response = "Usage: pow <base> <exponent>\n";
            }

        } else {
            response = "Unknown command\n";
        }

        send(client_fd, response.c_str(), response.size(), 0);
    }

    co_return;
}

