#include <iostream>
#include "httplib.h"
#include <string>
#include <map>
#include <nlohmann/json.hpp>

#include "bsoncxx/builder/basic/document.hpp"
#include "bsoncxx/json.hpp"
#include "mongocxx/client.hpp"
#include "mongocxx/instance.hpp"
#include "mongocxx/uri.hpp"




std::map<std::string, std::string> users;


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



void registerUser(const httplib::Request& req, httplib::Response& res) {
	std::string username = req.get_param_value("username");
	std::string password = req.get_param_value("password");
	std::string email = req.get_param_value("email");

	//if (users.find(username) != users.end()) {
	//	std::cout << "User exists" << std::endl;
	//	res.set_content("Пользователь уже существует", "text/plain");
	//	res.status = 400;
	//	return;
	//}

	//users[username] = password;

	std::cout << "username " << username << std::endl;
	std::cout << "password " << password << std::endl;
	std::cout << "email " << email << std::endl;
	res.set_header("Content-Type", "text/plain");
	res.set_content("Пользователь успешно зарегистрирован", "text/plain");

	res.status = 200;
	//return;

}

void homePage(const httplib::Request& req, httplib::Response& res) {
	//res.set_content("HomePage ", "text/plain");
	//res.status = 200;
	nlohmann::json response = {
		{"message", "Hello, world"}
	};

	res.set_header("Content-Type", "application/json");
	res.set_content(response.dump(), "application/json");
	res.status = 200;
}

void hi(const httplib::Request& req, httplib::Response& res) {
	res.set_header("Content-Type", "text/plain");
	res.set_content("Hello world", "text/plain");
	res.status = 200;
}

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

int main() {

    //подключение к базе mongodb
    //	
    httplib::Server svr;


    svr.Get("/hi", hi);

    svr.Get("/home", homePage);

    svr.Post("/register", registerUser);
    // Обрабатываем редирект с Google
    svr.Get("/auth/google/callback", googleAuthCallback);

    std::cout << "Server started at http://localhost:8080\n";
    //svr.listen("0.0.0.0", 5173);
    svr.listen("127.0.0.1", 8080);
    return 0;
}
