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


std::expected<void, std::string> write_line(int fd, const std::string& data) {
    std::string message = data + "\n";
    const char* ptr = message.data();
    size_t remaining = message.size();

    while (remaining > 0) {
        async_write(fd);

        ssize_t n = send(fd, ptr, remaining, 0);
        if (n <= 0) {
            return std::unexpected(std::string(strerror(errno)));
        }
        ptr += n;
        remaining -= n;
    }
    return {};
}

std::coroutine_handle<> handle_client(int client_fd) {
  struct local_promise_type {
    std::coroutine_handle<> get_return_object() {
      return std::coroutine_handle<local_promise_type>::from_promise(*this);
    }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() { std::terminate(); }
    io_operation await_transform(io_operation op) { return op; }
  };

  local_promise_type promise;
  return std::coroutine_handle<local_promise_type>::from_address(
      std::coroutine_handle<local_promise_type>::from_promise(promise).address());
}


int main() {
    // Создаем epoll
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        return 1;
    }

    // Создаем серверный сокет
    int server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    // Настраиваем адрес
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    addr.sin_addr.s_addr = INADDR_ANY;

    // Привязываем сокет
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    // Начинаем слушать
    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen");
        return 1;
    }

    // Добавляем серверный сокет в epoll
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        perror("epoll_ctl");
        return 1;
    }

    std::cout << "Server started on port 12345. Waiting for connections...\n";

    // Основной цикл событий
    std::vector<epoll_event> events(64);
    std::unordered_map<int, std::coroutine_handle<>> coroutines;
    std::queue<std::coroutine_handle<>> ready_coroutines;

    while (true) {
        int n = epoll_wait(epoll_fd, events.data(), events.size(), -1); // Block until events
        if (n < 0 && errno != EINTR) {
            perror("epoll_wait");
            continue;
        }

        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == server_fd) {
                // Новое подключение
                sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept4(server_fd, (struct sockaddr*)&client_addr, 
                                      &client_len, SOCK_NONBLOCK | O_NONBLOCK);
                if (client_fd < 0) {
                  perror("accept4");
                  continue;
                }

                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                std::cout << "New connection from " << client_ip << ":" 
                          << ntohs(client_addr.sin_port) << "\n";

                // Запускаем обработчик клиента
                auto handle = handle_client(client_fd);
                coroutines[client_fd] = handle;
                ready_coroutines.push(handle);
            } else {
              auto handle_ptr = events[i].data.ptr;
              auto handle = std::coroutine_handle<>::from_address(handle_ptr);
              ready_coroutines.push(handle);
            }
        }
        
        while (!ready_coroutines.empty()) {
          auto handle = ready_coroutines.front();
          ready_coroutines.pop();
          handle.resume();
          if (handle.done()) {
            for (auto it = coroutines.begin(); it != coroutines.end(); ++it) {
                if (it->second == handle) {
                    coroutines.erase(it);
                    break;
                }
            }
            handle.destroy();
          }
        }
    }

    close(server_fd);
    close(epoll_fd);
    return 0;
}
