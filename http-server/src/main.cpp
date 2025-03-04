#include <iostream>
#include "httplib.h"

void googleAuthCallback(const httplib::Request &req, httplib::Response &res) {
    if (req.has_param("code")) {
        std::string auth_code = req.get_param_value("code");
        std::cout << "Authorization Code: " << auth_code << std::endl;

        // Здесь можно сразу отправить запрос на получение access_token
        res.set_content("Received Authorization Code: " + auth_code, "text/plain");
    } else {
        res.status = 400;
        res.set_content("Error: No code received", "text/plain");
    }
}

int main() {
    httplib::Server svr;

    // Обрабатываем редирект с Google
    svr.Get("/auth/google/callback", googleAuthCallback);

    std::cout << "Server started at http://localhost:8080\n";
    svr.listen("0.0.0.0", 5173);

    return 0;
}
