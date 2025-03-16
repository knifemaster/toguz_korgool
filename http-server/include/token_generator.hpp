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


    bool isValidToken(const std::string& token) {
	std::lock_guard<std::mutex> lock(tokens_mutex);

	auto it = tokens.find(token);
        if (it == tokens.end()) {
            return false; // Токен не найден
        }

 
        auto now = std::chrono::system_clock::now();
        auto tokenCreationTime = it->second;
        auto elapsedTime = now - tokenCreationTime;

        if (elapsedTime > ttl) {
            tokens.erase(it);
            return false;
        }

        return true;
    }


    void removeToken(const std::string& token) {
        tokens.erase(token);
    }

    void cleanupExpiredTokens() {
    	auto now = std::chrono::system_clock::now();
	for (auto it = tokens.begin(); it != tokens.end();) {
		if (now - it->second > ttl) {
			it = tokens.erase(it);
		} else {
			++it;
		}
	}
    }
};

