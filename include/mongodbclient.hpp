#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/exception.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <iostream>
#include <string>
#include <optional>

class MongoDBClient {
public:
    // Конструктор для подключения к MongoDB
    MongoDBClient(const std::string& uri_str, const std::string& db_name, const std::string& collection_name)
        : instance_{}, client_{mongocxx::uri{uri_str}}, db_{client_[db_name]}, collection_{db_[collection_name]} {
        std::cout << "Connected to MongoDB!" << std::endl;
    }

    bool set_collection(std::string& collection) {
        if (db_[collection]) {
            return true;
        } else {
            return false;
        }
    }

    // Вставка документа в коллекцию
    bool insert_document(const bsoncxx::document::view_or_value& document) {
        try {
            auto result = collection_.insert_one(document);
            if (result) {
                std::cout << "Document inserted with id: " << result->inserted_id().get_oid().value.to_string() << std::endl;
                return true;
            } else {
                std::cerr << "Failed to insert document!" << std::endl;
                return false;
            }
        } catch (const mongocxx::exception& e) {
            std::cerr << "MongoDB error: " << e.what() << std::endl;
            return false;
        }
    }

    // Поиск документов в коллекции
    bool find_documents(const bsoncxx::document::view_or_value& filter = {}) {
        try {
            auto cursor = collection_.find(filter);
            if (cursor.begin() == cursor.end()) {
	    	return false;

	    }
	    
	    for (auto&& doc : cursor) {
            std::cout << "Found document: " << bsoncxx::to_json(doc) << std::endl;
		    return true;
        }

	    
        } catch (const mongocxx::exception& e) {
            std::cerr << "MongoDB error: " << e.what() << std::endl;
	        return false;
        }

	return false;	
    }


    bool find_document_get_value(const bsoncxx::document::view_or_value& filter, std::string& field, std::string& value) {
        try {
            auto cursor = collection_.find(filter);
            if (cursor.begin() == cursor.end()) {
                return false;
            }
            
            for (auto&& doc : cursor) {
                auto password = doc[field];

                if (password && password.type() == bsoncxx::type::k_string) {
                    std::cout << "password " << password.get_string().value << std::endl;
                    value = password.get_string().value;
                }

            }
            return true;

        } catch (const mongocxx::exception& e) {
            std::cerr << "MongoDB error: " << e.what() << std::endl;
            return false;
        }
    }

    // Обновление документов в коллекции
    bool update_documents(const bsoncxx::document::view_or_value& filter, const bsoncxx::document::view_or_value& update) {
        try {
            auto result = collection_.update_many(filter, update);
            std::cout << "Updated " << result->modified_count() << " documents." << std::endl;
            return true;
        } catch (const mongocxx::exception& e) {
            std::cerr << "MongoDB error: " << e.what() << std::endl;
            return false;
        }
    }

    // Удаление документов из коллекции
    bool delete_documents(const bsoncxx::document::view_or_value& filter) {
        try {
            auto result = collection_.delete_many(filter);
            std::cout << "Deleted " << result->deleted_count() << " documents." << std::endl;
            return true;
        } catch (const mongocxx::exception& e) {
            std::cerr << "MongoDB error: " << e.what() << std::endl;
            return false;
        }
        
    }

private:
    mongocxx::instance instance_;  // Экземпляр MongoDB (требуется для работы драйвера)
    mongocxx::client client_;      // Клиент для подключения к MongoDB
    mongocxx::database db_;        // База данных
    mongocxx::collection collection_;  // Коллекция
};

