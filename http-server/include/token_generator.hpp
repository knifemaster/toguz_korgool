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


