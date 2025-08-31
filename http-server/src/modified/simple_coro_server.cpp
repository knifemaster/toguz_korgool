#include <iostream>
#include <coroutine>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cstring>
#include <memory>
#include <fstream>

#include <sstream>
#include <chrono>
#include <ctime>
#include <cmath>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <mutex>

using json = nlohmann::json;

// Global configuration
struct ServerConfig {
    int port = 8080;
    int max_connections = 100;
    int timeout = 30;
    int max_request_size = 10240;
    bool rate_limiting_enabled = true;
    int requests_per_minute = 60;
    bool logging_enabled = true;
    
    static ServerConfig load_from_file(const std::string& config_file) {
        ServerConfig config;
        
        try {
            std::ifstream file(config_file);
            if (file.is_open()) {
                json j;
                file >> j;
                
                if (j.contains("server")) {
                    auto& server = j["server"];
                    if (server.contains("port")) config.port = server["port"];
                    if (server.contains("max_connections")) config.max_connections = server["max_connections"];
                    if (server.contains("timeout")) config.timeout = server["timeout"];
                }
                
                if (j.contains("limits")) {
                    auto& limits = j["limits"];
                    if (limits.contains("max_request_size")) config.max_request_size = limits["max_request_size"];
                    
                    if (limits.contains("rate_limit")) {
                        auto& rate_limit = limits["rate_limit"];
                        if (rate_limit.contains("enabled")) config.rate_limiting_enabled = rate_limit["enabled"];
                        if (rate_limit.contains("requests_per_minute")) config.requests_per_minute = rate_limit["requests_per_minute"];
                    }
                }
                
                if (j.contains("logging")) {
                    auto& logging = j["logging"];
                    if (logging.contains("enabled")) config.logging_enabled = logging["enabled"];
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error loading config file: " << e.what() << std::endl;
        }
        
        return config;
    }
};

// Rate limiting structure
struct ClientRateLimit {
    std::chrono::steady_clock::time_point last_reset;
    int request_count;
    
    ClientRateLimit() : last_reset(std::chrono::steady_clock::now()), request_count(0) {}
};

// Global configuration
ServerConfig g_config;

// Client rate limiting
std::unordered_map<int, ClientRateLimit> g_client_rates;
std::mutex g_rate_mutex;

constexpr int MAX_EVENTS = 10;

// Logging helper
void log_message(const std::string& level, const std::string& message) {
    if (!g_config.logging_enabled) return;
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::cout << "[" << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] " 
              << level << ": " << message << std::endl;
}

// Rate limiting check
bool check_rate_limit(int client_fd) {
    if (!g_config.rate_limiting_enabled) return true;
    
    std::lock_guard<std::mutex> lock(g_rate_mutex);
    
    auto now = std::chrono::steady_clock::now();
    auto& client_info = g_client_rates[client_fd];
    
    // Reset counter if a minute has passed
    if (std::chrono::duration_cast<std::chrono::minutes>(now - client_info.last_reset).count() >= 1) {
        client_info.last_reset = now;
        client_info.request_count = 0;
    }
    
    // Check if limit exceeded
    if (client_info.request_count >= g_config.requests_per_minute) {
        return false;
    }
    
    client_info.request_count++;
    return true;
}

// Clean up rate limit info for disconnected client
void cleanup_client_rate_limit(int client_fd) {
    if (!g_config.rate_limiting_enabled) return;
    
    std::lock_guard<std::mutex> lock(g_rate_mutex);
    g_client_rates.erase(client_fd);
}

// Утилита для установки non-blocking сокета
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
    }
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
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
            perror("epoll_ctl EPOLL_CTL_ADD");
        }
    }

    void await_resume() const noexcept {}
};

// Задача с правильной обработкой lifetime
struct Task {
    struct promise_type {
        std::coroutine_handle<> handle;
        
        Task get_return_object() { 
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)}; 
        }
        
        std::suspend_never initial_suspend() { return {}; }
        
        auto final_suspend() noexcept {
            struct Awaiter {
                std::coroutine_handle<> handle;
                bool await_ready() const noexcept { return false; }
                void await_suspend(std::coroutine_handle<>) const noexcept {
                    if (handle) handle.destroy();
                }
                void await_resume() const noexcept {}
            };
            return Awaiter{handle};
        }
        
        void return_void() {}
        void unhandled_exception() { 
            try {
                std::rethrow_exception(std::current_exception());
            } catch (const std::exception& e) {
                std::cerr << "Unhandled exception: " << e.what() << std::endl;
            }
            std::terminate(); 
        }
        
        void set_handle(std::coroutine_handle<> h) {
            handle = h;
        }
    };
    
    Task(std::coroutine_handle<promise_type> h) {
        h.promise().set_handle(h);
    }
    
    Task(Task&& t) = default;
};

// Process command with JSON protocol
json process_json_command(const json& request) {
    json response;
    
    // Copy the request ID to the response
    if (request.contains("id")) {
        response["id"] = request["id"];
    }
    
    // Validate request structure
    if (!request.is_object()) {
        response["status"] = "error";
        response["error"] = {{"code", 400}, {"message", "Request must be a JSON object"}};
        return response;
    }
    
    // Check if command exists
    if (!request.contains("command")) {
        response["status"] = "error";
        response["error"] = {{"code", 400}, {"message", "Missing command field"}};
        return response;
    }
    
    // Validate command is a string
    if (!request["command"].is_string()) {
        response["status"] = "error";
        response["error"] = {{"code", 400}, {"message", "Command must be a string"}};
        return response;
    }
    
    std::string command = request["command"];
    
    try {
        if (command == "echo") {
            if (request.contains("parameters")) {
                if (!request["parameters"].is_object()) {
                    response["status"] = "error";
                    response["error"] = {{"code", 400}, {"message", "Parameters must be a JSON object"}};
                    return response;
                }
                
                if (request["parameters"].contains("text")) {
                    if (!request["parameters"]["text"].is_string()) {
                        response["status"] = "error";
                        response["error"] = {{"code", 400}, {"message", "Text parameter must be a string"}};
                        return response;
                    }
                    response["status"] = "success";
                    response["result"] = request["parameters"]["text"];
                } else {
                    response["status"] = "error";
                    response["error"] = {{"code", 400}, {"message", "Missing text parameter for echo command"}};
                }
            } else {
                response["status"] = "error";
                response["error"] = {{"code", 400}, {"message", "Missing parameters for echo command"}};
            }
            
        } else if (command == "time") {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::ostringstream oss;
            oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
            response["status"] = "success";
            response["result"] = oss.str();
            
        } else if (command == "message") {
            response["status"] = "success";
            response["result"] = "Server says hello!";
            
        } else if (command == "pow") {
            if (request.contains("parameters")) {
                if (!request["parameters"].is_object()) {
                    response["status"] = "error";
                    response["error"] = {{"code", 400}, {"message", "Parameters must be a JSON object"}};
                    return response;
                }
                
                if (request["parameters"].contains("base") && request["parameters"].contains("exponent")) {
                    if (!request["parameters"]["base"].is_number() || !request["parameters"]["exponent"].is_number()) {
                        response["status"] = "error";
                        response["error"] = {{"code", 400}, {"message", "Base and exponent must be numbers"}};
                        return response;
                    }
                    
                    double base = request["parameters"]["base"];
                    double exp = request["parameters"]["exponent"];
                    double result = std::pow(base, exp);
                    response["status"] = "success";
                    response["result"] = result;
                } else {
                    response["status"] = "error";
                    response["error"] = {{"code", 400}, {"message", "Missing base or exponent parameter for pow command"}};
                }
            } else {
                response["status"] = "error";
                response["error"] = {{"code", 400}, {"message", "Missing parameters for pow command"}};
            }
            
        } else if (command == "exit") {
            response["status"] = "success";
            response["result"] = "Goodbye!";
            
        } else {
            response["status"] = "error";
            response["error"] = {{"code", 404}, {"message", "Unknown command: " + command}};
        }
    } catch (const std::exception& e) {
        response["status"] = "error";
        response["error"] = {{"code", 500}, {"message", "Internal server error: " + std::string(e.what())}};
    }
    
    return response;
}

// Process command with text protocol (backward compatibility)
std::pair<std::string, bool> process_text_command(const std::string& input) {
    std::istringstream iss(input);
    std::string command;
    iss >> command;
    
    std::string response;
    bool should_exit = false;

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

    } else if (command == "exit") {
        response = "Goodbye!\n";
        should_exit = true;

    } else {
        response = "Unknown command\nAvailable commands: echo, time, message, pow, exit\n";
    }
    
    return {response, should_exit};
}

// Обработка клиента с циклом сообщений
Task client_coroutine(int epoll_fd, int client_fd) {
    char buffer[1024] = {0};
    log_message("INFO", "New client connected: " + std::to_string(client_fd));

    while (true) {
        // Ждём данные от клиента
        co_await EpollAwaiter{epoll_fd, client_fd, EPOLLIN};

        int valread = read(client_fd, buffer, sizeof(buffer) - 1);
        if (valread <= 0) {
            if (valread < 0) {
                log_message("ERROR", "Read error on client " + std::to_string(client_fd) + ": " + strerror(errno));
            } else {
                log_message("INFO", "Client " + std::to_string(client_fd) + " disconnected.");
            }
            cleanup_client_rate_limit(client_fd);
            close(client_fd);
            break;
        }

        // Check request size
        if (valread > g_config.max_request_size) {
            std::string error_response = "{\"status\":\"error\",\"error\":{\"code\":413,\"message\":\"Request too large\"}}\n";
            send(client_fd, error_response.c_str(), error_response.size(), 0);
            continue;
        }

        buffer[valread] = '\0';
        std::string input(buffer);
        
        // Trim whitespace from input
        input.erase(0, input.find_first_not_of(" \t\r\n"));
        input.erase(input.find_last_not_of(" \t\r\n") + 1);
        
        // Check rate limiting
        if (!check_rate_limit(client_fd)) {
            std::string error_response = "{\"status\":\"error\",\"error\":{\"code\":429,\"message\":\"Rate limit exceeded\"}}\n";
            send(client_fd, error_response.c_str(), error_response.size(), 0);
            continue;
        }
        
        // Check if input is JSON
        bool is_json = !input.empty() && (input[0] == '{' || input[0] == '[');
        
        if (is_json) {
            try {
                json request = json::parse(input);
                json response = process_json_command(request);
                
                std::string response_str = response.dump() + "\n";
                ssize_t sent = send(client_fd, response_str.c_str(), response_str.size(), 0);
                if (sent == -1) {
                    log_message("ERROR", "Send error on client " + std::to_string(client_fd) + ": " + strerror(errno));
                    cleanup_client_rate_limit(client_fd);
                    close(client_fd);
                    break;
                }
                
                // Check if client wants to exit
                if (request.contains("command") && request["command"] == "exit") {
                    log_message("INFO", "Client " + std::to_string(client_fd) + " requested disconnect.");
                    cleanup_client_rate_limit(client_fd);
                    close(client_fd);
                    break;
                }
            } catch (const json::parse_error& e) {
                std::string error_response = "{\"status\":\"error\",\"error\":{\"code\":400,\"message\":\"Invalid JSON: " + std::string(e.what()) + "\"}}\n";
                send(client_fd, error_response.c_str(), error_response.size(), 0);
            }
        } else {
            // Process as text command (backward compatibility)
            auto [response, should_exit] = process_text_command(input);
            
            ssize_t sent = send(client_fd, response.c_str(), response.size(), 0);
            if (sent == -1) {
                log_message("ERROR", "Send error on client " + std::to_string(client_fd) + ": " + strerror(errno));
                cleanup_client_rate_limit(client_fd);
                close(client_fd);
                break;
            }
            
            if (should_exit) {
                log_message("INFO", "Client " + std::to_string(client_fd) + " requested disconnect.");
                cleanup_client_rate_limit(client_fd);
                close(client_fd);
                break;
            }
        }
    }

    co_return;
}

int main(int argc, char* argv[]) {
    // Load configuration
    std::string config_file = "server_config.json";
    if (argc > 1) {
        config_file = argv[1];
    }
    
    g_config = ServerConfig::load_from_file(config_file);
    
    int server_fd, epoll_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Создаем сокет
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket creation failed");
        return 1;
    }
    
    // Настраиваем сокет
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        close(server_fd);
        return 1;
    }
    
    set_nonblocking(server_fd);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(g_config.port);

    // Привязываем сокет
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return 1;
    }
    
    // Начинаем слушать
    if (listen(server_fd, g_config.max_connections) < 0) {
        perror("listen failed");
        close(server_fd);
        return 1;
    }

    // Создаем epoll
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        close(server_fd);
        return 1;
    }

    // Регистрируем серверный сокет
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl server_fd failed");
        close(server_fd);
        close(epoll_fd);
        return 1;
    }

    log_message("INFO", "Coroutine server listening on port " + std::to_string(g_config.port));

    epoll_event events[MAX_EVENTS];

    while (true) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n == -1) {
            if (errno == EINTR) {
                continue; // Прервано сигналом, продолжаем
            }
            perror("epoll_wait failed");
            break;
        }

        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == server_fd) {
                // Новое подключение
                int client_fd = accept(server_fd, nullptr, nullptr);
                if (client_fd >= 0) {
                    set_nonblocking(client_fd);
                    client_coroutine(epoll_fd, client_fd);
                } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("accept failed");
                }
            } else {
                // Событие клиента
                auto* awaiter = static_cast<EpollAwaiter*>(events[i].data.ptr);
                if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, awaiter->fd, nullptr) == -1) {
                    perror("epoll_ctl EPOLL_CTL_DEL failed");
                }
                awaiter->handle.resume();
            }
        }
    }

    close(server_fd);
    close(epoll_fd);
    return 0;
}
