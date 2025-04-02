#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <ctime>
#include <string>


constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 10;
constexpr int BUFFER_SIZE = 1024;

// Установка сокета в неблокирующий режим
void set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl get");
        exit(1);
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl set");
        exit(1);
    }
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        return 1;


    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return 1;
    }

    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen failed");
        return 1;
    }

    set_nonblocking(server_fd);

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        return 1;
    }


    epoll_event event{}, events[MAX_EVENTS];
    event.data.fd = server_fd;
    event.events = EPOLLIN;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

    std::cout << "Сервер запущен на порту " << PORT << std::endl;

    while (true) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == server_fd) {
                while (true) {
                    int client_fd = accept(server_fd, nullptr, nullptr);
                    if (client_fd == -1) {
                        break;
                    }
                    set_nonblocking(client_fd);

                    epoll_event client_event{};
                    client_event.data.fd = client_fd;
                    client_event.events = EPOLLIN | EPOLLET; // Edge-triggered
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event);

                    std::cout << "Новый клиент: fd=" << client_fd << std::endl;
                }
            } else {
                int client_fd = events[i].data.fd;
                while (true) {
                    char buffer[BUFFER_SIZE];
                    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
                    if (bytes_read <= 0) {
                        if (errno != EAGAIN && errno != EWOULDBLOCK) {
                            std::cout << "Отключен клиент: fd=" << client_fd << std::endl;
                            close(client_fd);
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
                        }
                        break;
                    }

                    buffer[bytes_read] = '\0';
                    std::string input(buffer);
                    input.erase(input.find_last_not_of(" \n\r\t") + 1); // trim

                    std::cout << "[" << client_fd << "] " << input << std::endl;

                    std::string response;

                    if (input == "PING") {
                        response = "PONG\n";
                    } else if (input == "TIME") {
                        time_t now = time(nullptr);
                        response = std::string("TIME: ") + ctime(&now);
                    } else if (input == "EXIT") {
                        response = "Goodbye!\n";
                        send(client_fd, response.c_str(), response.length(), 0);
                        close(client_fd);
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
                        std::cout << "Клиент вышел: fd=" << client_fd << std::endl;
                        break;
                    } else {
                        response = "Echo: " + input + "\n";
                    }

                    send(client_fd, response.c_str(), response.length(), 0);
                }
            }
        }
    }

    close(server_fd);


    return 0;
}
