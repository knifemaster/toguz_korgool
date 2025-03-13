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


