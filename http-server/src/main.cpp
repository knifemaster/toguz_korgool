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
#include <sha256.hpp>
#include <token_generator.hpp>
#include <email_sender.hpp>


int main() {

    try {

        TokenGenerator tokenGenerator(16, std::chrono::seconds(10));

        std::string token = tokenGenerator.generateToken();
        std::cout << "Сгенерированный токен: " << token << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }


	DotEnv env;

	std::string dbUrl = env.get("DATABASE_URL");
	std::string database_name = env.get("DATABASE");
	std::string collection = env.get("COLLECTION");

	MongoDBClient client(dbUrl, database_name, collection);

	httplib::Server svr;


	svr.Post("/register", [&](const httplib::Request& req, httplib::Response& res) {
			const std::string username = req.get_param_value("username");
			std::string password = req.get_param_value("password");
			std::string email = req.get_param_value("email");


			auto filter_username = bsoncxx::builder::stream::document{} << "username" << username << bsoncxx::builder::stream::finalize;
			auto filter_email = bsoncxx::builder::stream::document{} << "email" << email << bsoncxx::builder::stream::finalize;


			if (client.find_documents(filter_username.view()) || client.find_documents(filter_email.view())) {
				std::cout << "Логин или email существует" << '\n';
			} else {

				auto builder = bsoncxx::builder::stream::document{};
				auto now = std::chrono::system_clock::now();
				auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

				SHA256 sha256;
				std::string cipher_password = sha256.hash(password);

				bsoncxx::document::value doc_value = builder
					<< "username" << username
					<< "password" << cipher_password
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


	svr.Post("/login", [&](const httplib::Request& req, httplib::Response& res) {
		
			std::string email = req.get_param_value("email");
			std::string password = req.get_param_value("password");

			auto filter_email = bsoncxx::builder::stream::document{} << "email" << email << bsoncxx::builder::stream::finalize;

			std::string cipher_password;

			std::string field = "password";

			if (!client.find_document_get_value(filter_email.view(), field, cipher_password)) {
				std::cout << "email нету в базе" << std::endl;

			} else {
				std::cout << "email есть в базе" << std::endl;
				std::cout << cipher_password << std::endl;
				SHA256 sha256;
				
				std::cout << "hash|"<< sha256.hash(password) << std::endl;
				std::cout << "hash|"<< cipher_password << std::endl;

				if (sha256.hash(password) == cipher_password) {
					std::cout << "password correct" << std::endl;
					res.status = 200;
				
				}
				else {
					std::cout << "password incorrect" << std::endl;
					
				}
			}
			
	});


/*
	svr.Get("/reset/:token", [&](const httplib::Request& req, httplib::Response& res) {
		auto reset_token = req.path_params.at("token");
		
		//res.set_content(user_id, "text/plain");
		std::cout << reset_token << std::endl;
	

	});



	svr.Post("/reset_pass", [&](const httplib::Request& req, httplib::Response& res) {
		std::string email = req.get_param_value("email");
		std::string token_for_reset;

		
		try {
        ///TokenGenerator tokenGenerator;

			std::string token = tokenGenerator.generateToken(16);
			token_for_reset = token;
			std::cout << "Сгенерированный токен: " << token << std::endl;
		} catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
		}

		std::string url = + "http://localhost:8080/"+token_for_reset;

		//отправка email
		std::string from = "";
		std::string to = email;
		std::string username = "";
		std::string password = "";

		EmailSender sender(from, to, username, password);

		std::string subject = "Password reset from toguz korgool";
		std::string body = url;

		if (sender.send(subject, body)) {
			std::cout << "Email sent" << std::endl;
		}
		else {
			std::cout << "failed to sent email" << std::endl;
		}
		

	});

*/

	std::cout << "Server started at http://localhost:8080\n";
	svr.listen("127.0.0.1", 8080);
	return 0;
}
