#include <iostream>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <mutex>


class TokenGenerator {
private:
    
    std::unordered_map<std::string, std::chrono::system_clock::time_point> tokens;
    std::mutex tokens_mutex;
    size_t tokenLength;
    std::chrono::seconds ttl;

public:

    TokenGenerator(size_t length, std::chrono::seconds tokenTTL)
        : tokenLength(length), ttl(tokenTTL) {
        if (RAND_status() == 0) {
            throw std::runtime_error("Ошибка инициализации OpenSSL");
        }
    }


    std::string generateToken() {
	std::lock_guard<std::mutex> lock(tokens_mutex);

        unsigned char buffer[tokenLength];
        if (RAND_bytes(buffer, tokenLength) != 1) {
            throw std::runtime_error("Ошибка генерации случайных данных");
        }

        std::stringstream ss;
        for (size_t i = 0; i < tokenLength; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i];
        }

        std::string token = ss.str();

        tokens[token] = std::chrono::system_clock::now();

        return token;
    }

