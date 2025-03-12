#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/exception.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <iostream>
#include <string>


class MongoDBClient {
public:
    
    MongoDBClient(const std::string& uri_str, const std::string& db_name, const std::string& collection_name)
        : instance_{}, client_{mongocxx::uri{uri_str}}, db_{client_[db_name]}, collection_{db_[collection_name]} {
        std::cout << "Connected to MongoDB!" << std::endl;
    }


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
