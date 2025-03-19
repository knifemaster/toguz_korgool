#include <httplib.h>
#include <jwt-cpp/jwt.h>
#include <map>
#include <set>
#include <chrono>


class JWTManager {
public:
    JWTManager(const std::string& secret_key) : secret_key_(secret_key) {}

    // Создание JWT
    std::string create_token(const std::string& username, const std::chrono::seconds& expires_in) {
        auto token = jwt::create()
            .set_issuer("auth0")
            .set_type("JWT")
            .set_payload_claim("username", jwt::claim(username))
            .set_issued_at(std::chrono::system_clock::now())
            .set_expires_at(std::chrono::system_clock::now() + expires_in)
            .sign(jwt::algorithm::hs256{secret_key_});

        return token;
    }


    bool verify_token(const std::string& token, std::string& username) {
        try {
            auto decoded = jwt::decode(token);
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{secret_key_})
                .with_issuer("auth0");

            verifier.verify(decoded);

            username = decoded.get_payload_claim("username").as_string();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "JWT verification failed: " << e.what() << std::endl;
            return false;
        }
    }

private:
    std::string secret_key_;
};
