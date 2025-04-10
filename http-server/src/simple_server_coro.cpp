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

#include "match_maker.hpp"
#include "game_manager2.hpp"

#include "bimap.hpp"


constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 10;


std::atomic<int> game_id_count = 0;

int generate_game_id() {
    return game_id_count.fetch_add(1, std::memory_order_relaxed);
}

//std::unordered_map <int, Player>
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

// Минимальная задача
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
Task client_coroutine(int epoll_fd, int client_fd, GameManager& manager, Bimap<int, int>& sockets_descriptors, ThreadSafeMatchmaker& matchmaker) {
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
        std::string response2;

        if (command == "find_game") {
            std::string id;
            //std::getline(iss, id);
            std::string rating;
            //std::getline(iss, rating);
            //int id_int = std::stoi(id);
            //int rating_int = std::stoi(rating);

            int id_int, rating_int;
            if (iss >> id_int >> rating_int) {
                std::cout << "id:" << id_int << std::endl;
                std::cout << "rating:" << rating_int << std::endl;

            }

   

        //matchmaker
        //matchmaker.addPlayer
            matchmaker.addPlayer({
                client_fd,
                id_int,
                rating_int,
                std::chrono::system_clock::now()
            });

            std::pair<Player, Player> match;
            for (const auto& player : matchmaker.get_players()) {
                std::cout << player.client_fd << " " << player.id << " " << player.rating << std::endl;    
            }
            
            if (matchmaker.tryFindMatch(match)) {
                std::cout << "in matchmaker if" << std::endl;
                // нашли игроков
                

                std::cout << match.first.id << " " << match.second.id << std::endl;
                std::cout << match.first.rating << " " << match.second.rating << std::endl;
                response = "game finded id " + std::to_string(match.first.client_fd);
                response2 = "game finded id " + std::to_string(match.second.client_fd);
                send(match.first.client_fd, response.c_str(), response.size(), 0);
                send(match.second.client_fd, response2.c_str(), response2.size(), 0);
                //
                // В реальном приложении здесь была бы логика старта игры
                std::cout << "end of matchmaker if" << std::endl;
                continue;
            }
            //matchmaker.matchingThread();
            
           response = "in queue";
       
        // finding games and add player id
        // and remember socket ids
        // remeber game_id
        }

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

        }// else {
         //   response = "Unknown command\n";
        //}

        send(client_fd, response.c_str(), response.size(), 0);
    }

    co_return;
}


void init_socket_server() {

   //ThreadSafeMatchmaker match_maker;
   //    std::thread matching_thread([&]() {
   //     match_maker.matchingThread();
   // });

    GameManager manager;
    Bimap<int, int> socket_descriptors;
    ThreadSafeMatchmaker matchmaker;
    
  

    manager.create_game(generate_game_id());

    int server_fd, epoll_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    set_nonblocking(server_fd);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);

    epoll_fd = epoll_create1(0);

    // Регистрируем серверный сокет
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

    std::cout << "Coroutine server listening on port " << PORT << "...\n";

    epoll_event events[MAX_EVENTS];

    while (true) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == server_fd) {
                // Новое подключение
                int client_fd = accept(server_fd, nullptr, nullptr);
                if (client_fd >= 0) {
                    set_nonblocking(client_fd);
                    client_coroutine(epoll_fd, client_fd, manager, socket_descriptors, matchmaker);
                }
            } else {
                // Событие клиента
                auto* awaiter = static_cast<EpollAwaiter*>(events[i].data.ptr);
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, awaiter->fd, nullptr);
                awaiter->handle.resume();
            }
        }
    }

    close(server_fd);
    close(epoll_fd);
}

int main() {

    init_socket_server();
 
    return 0;
}

