#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <cstdlib>

class DotEnv {
    private:
        std::map<std::string, std::string> envMap;

        void parseDotEnv(const std::string& filePath) {
            std::ifstream file(filePath);
            std::string line;

            while (std::getline(file, line)) {
                if (line.empty() || line[0] == '#') continue;

                std::istringstream iss(line);
                std::string key, value;
                if (std::getline(iss, key, '=') && std::getline(iss, value)) {
                    value.erase(0, value.find_first_not_of(" \t\n\r\"'"));
                    value.erase(value.find_last_not_of(" \t\n\r\"'") + 1);
                    envMap[key] = value;
                }
            }
        }

   public:

        DotEnv(const std::string& filePath = ".env") {
            std::ifstream file(filePath);
            if (!file.is_open()) {
                std::cerr << "Error: Could not open .env file!" << std::endl;
                return;
            }
            parseDotEnv(filePath);
            load();
        }

        void load() {
            for (const auto& [key, value] : envMap) {
                setenv(key.c_str(), value.c_str(), 1);
            }
        }

        std::string get(const std::string& key, const std::string& defaultValue = "") const {
            auto it = envMap.find(key);
            if (it != envMap.end()) {
                return it->second;
            }
            return defaultValue;
        }

        bool has(const std::string& key) const {
            return envMap.find(key) != envMap.end();
        }
};
