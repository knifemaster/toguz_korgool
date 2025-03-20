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

#include <token_generator.hpp>
#include <email_sender.hpp>
#include <jwt_manager.hpp>
#include <sha256.hpp>

std::string read_file(const std::string filename) {
	std::ifstream file(filename, std::ios::binary);

	if(!file.is_open()) {
		throw std::runtime_error("Couldn't open file " + filename);
	
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();

}

std::string get_mime_type(const std::string& file_path) {
    if (file_path.find(".html") != std::string::npos) {
        return "text/html";
    } else if (file_path.find(".css") != std::string::npos) {
        return "text/css";
    } else if (file_path.find(".js") != std::string::npos) {
        return "application/javascript";
    } else if (file_path.find(".png") != std::string::npos) {
        return "image/png";
    } else if (file_path.find(".jpg") != std::string::npos) {
        return "image/jpeg";
    } else {
        return "text/plain";
    }
}


bool is_user_agent_browser(const std::string& user_agent) {
	    //auto user_agent = req.get_header_value("User-Agent");
		std::cout << "клиент зашел с " << user_agent << std::endl;

        if (user_agent.find("Mozilla") != std::string::npos ||
            user_agent.find("Chrome") != std::string::npos ||
            user_agent.find("Safari") != std::string::npos ||
            user_agent.find("Edge") != std::string::npos ||
            user_agent.find("Opera") != std::string::npos) {
			
			return true;
		 //   res.set_content("Hello, browser user!", "text/plain");
        } else {
			return false;
		 //   res.set_content("Hello, non-browser client!", "text/plain");
		}
}




int main() {

	TokenGenerator tokenGenerator(16, std::chrono::seconds(600));
    try {
        std::string token = tokenGenerator.generateToken();
        std::cout << "Сгенерированный токен: " << token << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

	DotEnv env;


	JWTManager jwt_manager("your_secret_key");
	std::set<std::string> active_refresh_tokens;


	std::string dbUrl = env.get("DATABASE_URL");
	std::string database_name = env.get("DATABASE");
	std::string collection = env.get("COLLECTION");
	
	std::unordered_map <std::string, std::string> emails_for_reset;


	MongoDBClient client(dbUrl, database_name, collection);

	httplib::Server svr;


	svr.Post("/register", [&client](const httplib::Request& req, httplib::Response& res) {

			if (is_user_agent_browser(req.get_header_value("User-Agent"))) {
				std::cout << "browser" << std::endl;

			} else {


			}

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

				utils::SHA256 sha256;
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


    svr.Post("/refresh", [&active_refresh_tokens, &jwt_manager](const httplib::Request& req, httplib::Response& res) {
        // Парсим JSON с refresh-токеном
        picojson::value body;
        std::string err = picojson::parse(body, req.body);
        if (!err.empty()) {
            res.status = 400; // Bad Request
            res.set_content("Invalid JSON", "text/plain");
            return;
        }

        std::string refresh_token = body.get("refresh_token").to_str();

        // Проверяем, что refresh-токен активен
        if (active_refresh_tokens.find(refresh_token) == active_refresh_tokens.end()) {
            res.status = 401; // Unauthorized
            res.set_content("Invalid refresh token", "text/plain");
            return;
        }

        // Проверяем refresh-токен
        std::string username;
        if (!jwt_manager.verify_token(refresh_token, username)) {
            res.status = 401; // Unauthorized
            res.set_content("Invalid refresh token", "text/plain");
            return;
        }

        // Удаляем использованный refresh-токен
        active_refresh_tokens.erase(refresh_token);

        // Создаём новый access-токен
        std::string new_access_token = jwt_manager.create_token(username, std::chrono::minutes{15});

        // Создаём новый refresh-токен
        std::string new_refresh_token = jwt_manager.create_token(username, std::chrono::hours{24 * 7});
        active_refresh_tokens.insert(new_refresh_token); // Сохраняем новый refresh-токен

        // Возвращаем новые токены клиенту
        picojson::object response;
        response["access_token"] = picojson::value(new_access_token);
        response["refresh_token"] = picojson::value(new_refresh_token);
        res.set_content(picojson::value(response).serialize(), "application/json");
    });



	svr.Get("/test", [&](const httplib::Request& req, httplib::Response& res) {
		 std::string auth_header = req.get_header_value("Authorization");

		 std::cout <<"|auth header |"<< auth_header << std::endl;

        // Проверяем, что заголовок начинается с "Bearer "
        if (auth_header.find("Bearer ") == 0) {
            std::string token = auth_header.substr(7); // Извлекаем токен
            std::string username;

            // Проверяем токен
            if (jwt_manager.verify_token(token, username)) {
                res.set_content("Hello, " + username + "! This is a protected resource.", "text/plain");
            } else {
                res.status = 401; // Unauthorized
                res.set_content("Invalid token", "text/plain");
            }
        } else {
            res.status = 401; // Unauthorized
            res.set_content("Authorization header missing or invalid", "text/plain");
        }

	});




	svr.Post("/login", [&client, &jwt_manager, &active_refresh_tokens](const httplib::Request& req, httplib::Response& res) {


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
				utils::SHA256 sha256;
				
				std::cout << "hash|"<< sha256.hash(password) << std::endl;
				std::cout << "hash|"<< cipher_password << std::endl;

				if (sha256.hash(password) == cipher_password) {
					
					std::cout << "password correct" << std::endl;
					res.status = 200;

					std::string access_token = jwt_manager.create_token(email, std::chrono::minutes{15});

					// Создаём refresh-токен (срок действия: 7 дней)
					std::string refresh_token = jwt_manager.create_token(email, std::chrono::hours{24 * 7});
					active_refresh_tokens.insert(refresh_token); // Сохраняем refresh-токен

					// Возвращаем токены клиенту
					picojson::object response;
					response["access_token"] = picojson::value(access_token);
					response["refresh_token"] = picojson::value(refresh_token);
					res.set_content(picojson::value(response).serialize(), "application/json");

				} else {
					res.status = 401; // Unauthorized
					res.set_content("Invalid username or password", "text/plain");
				}

			}

			
	});


	svr.Post("/save_password", [&client, &emails_for_reset](const httplib::Request& req, httplib::Response& res) {
		

		std::string referer = req.get_header_value("Referer");

		std::string token_referer;// = referer.substr(7); 
		size_t reset_pos = referer.find("/reset/");
		if (reset_pos != std::string::npos) {
			size_t token_start = reset_pos + 7; // 7 — длина "/reset/"
			
			token_referer = referer.substr(token_start);

			std::cout << "Токен: " << token_referer << std::endl;
		} else {
			std::cout << "Токен не найден в строке." << std::endl;
		}


		std::string email = emails_for_reset[token_referer];

		emails_for_reset.erase(token_referer);

		//auto email = req.get_param_value("email");
		auto new_password = req.get_param_value("password");
		auto confirm_password = req.get_param_value("confirm_password");

		//auto token = req.get_header_value("token");
		std::cout << "|email " << email << std::endl;
		std::cout << "|password " << new_password << std::endl;
		std::cout << "|confirm" << confirm_password << std::endl;

		if (new_password == confirm_password) {
			
			utils::SHA256 sha256;
			std::string cipher_password = sha256.hash(new_password);

			auto filter = bsoncxx::builder::stream::document{}
						<< "email" << email
						<< bsoncxx::builder::stream::finalize;

			auto update = bsoncxx::builder::stream::document{}
						<< "$set" << bsoncxx::builder::stream::open_document
						<< "password" << cipher_password
						<< bsoncxx::builder::stream::close_document
						<< bsoncxx::builder::stream::finalize;


			bool success = client.update_documents(filter.view(), update.view());

			if (success) {
				std::cout << "Пароль успешно обновлен." << std::endl;
			} else {
				std::cerr << "Не удалось обновить пароль." << std::endl;
			}
			std::cout << "Пароли равны" << std::endl;


		} else {

			std::cout << "Пароли не равны" << std::endl;
		}

	});



	svr.Get("/reset/:token", [&tokenGenerator, &emails_for_reset](const httplib::Request& req, httplib::Response& res) {
		auto reset_token = req.path_params.at("token");
		
		if (tokenGenerator.isValidToken(reset_token)) {
						
			//emails_for_reset.insert("token");
			std::string html_content = read_file("reset_password.html");
            res.set_content(html_content, get_mime_type("reset_password.html"));
			
 			httplib::Headers headers = {
       			 {"token", reset_token}
    		};
						
			res.set_header("token", reset_token);
			res.status = 200;
			

			std::cout << "Токен действителен." << std::endl;
        } else {
            std::cout << "Токен недействителен." << std::endl;
			res.status = 500;

        }
		std::cout << reset_token << std::endl;
	
	});


	svr.Get("/styles.css", [](const httplib::Request& req, httplib::Response& res) {
       	try {
           		std::string css_content = read_file("styles.css");
           		res.set_content(css_content, get_mime_type("styles.css"));
       	} catch (const std::exception& e) {
           	res.status = 500;
           //	res.set_content("Internal Server Error: " + std::string(e.what()), "text/plain");
     	}
    });

	svr.Post("/reset_pass", [&env, &tokenGenerator, &emails_for_reset](const httplib::Request& req, httplib::Response& res) {

		std::string token_for_reset = tokenGenerator.generateToken();

		
		std::string url = + "http://localhost:8080/reset/"+token_for_reset;

		//отправка email
		std::string from = env.get("EMAIL_FOR_SEND");
		std::string to = req.get_param_value("email");
		std::string username = env.get("EMAIL_FOR_SEND");
		std::string password = env.get("PASSWORD_FOR_EMAIL");	
		
		emails_for_reset[token_for_reset] = to;

		EmailSender sender(from, to, username, password);

		std::string subject = "Password reset from toguz korgool";
		std::string body = "Перейдите по ссылке для сброса пароля " + url;

		if (sender.send(subject, body)) {
			std::cout << "Email sent" << std::endl;
		}
		else {
			std::cout << "failed to sent email" << std::endl;
		}
		

	});



	std::cout << "Server started at http://localhost:8080\n";
	svr.listen("127.0.0.1", 8080);
	return 0;
}
