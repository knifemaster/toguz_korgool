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


