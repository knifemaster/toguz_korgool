#include <iostream>
#include "httplib.h"
#include <string>
#include <map>
#include <nlohmann/json.hpp>

#include "bsoncxx/json.hpp"
#include "mongocxx/client.hpp"
#include "mongocxx/instance.hpp"
#include "mongocxx/uri.hpp"

#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <load_env.hpp>
#include <mongodbclient.hpp>
#include <chrono>


int main() {

	DotEnv env;

    std::string dbUrl = env.get("DATABASE_URL");
	std::string database_name = env.get("DATABASE");
	std::string collection = env.get("COLLECTION");

	MongoDBClient client(dbUrl, database_name, collection);
	
    httplib::Server svr;


    svr.Post("/register", [&](const httplib::Request& req, httplib::Response& res) {
		std::string username = req.get_param_value("username");
		std::string password = req.get_param_value("password");
		std::string email = req.get_param_value("email");


		auto filter_username = bsoncxx::builder::stream::document{} << "username" << username << bsoncxx::builder::stream::finalize;
		auto filter_email = bsoncxx::builder::stream::document{} << "email" << email << bsoncxx::builder::stream::finalize;


		if (client.find_documents(filter_username.view()) || client.find_documents(filter_email.view())) {
			std::cout << "Логин или email существует" << '\n';
		}
		else {

			auto builder = bsoncxx::builder::stream::document{};
			auto now = std::chrono::system_clock::now();
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

        	bsoncxx::document::value doc_value = builder
				<< "username" << username
				<< "password" << password
				<< "email" << email
				<< "createdAt" << bsoncxx::types::b_date{now_ms}
				<< "type" << 1
				<< "rating" << 1200
				<< "gamesPlayed" << 0
				<< "wins" << 0
				<< "loses" << 0 
				<< bsoncxx::builder::stream::finalize;

        	client.insert_document(doc_value.view());
			std::cout << "юзер зарегистрирован" << '\n';
			}

    });
   

    std::cout << "Server started at http://localhost:8080\n";
    svr.listen("127.0.0.1", 8080);
    return 0;
}

