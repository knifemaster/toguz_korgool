cmake_minimum_required(VERSION 3.10)
project(GoogleOAuth2Example)

# Устанавливаем стандарт C++ (например, C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ищем библиотеку cURL
find_package(CURL REQUIRED)

# Ищем библиотеку nlohmann/json
find_package(nlohmann_json REQUIRED)

# Добавляем исполняемый файл
add_executable(GoogleOAuth2Example main.cpp)

# Подключаем библиотеки
target_link_libraries(GoogleOAuth2Example PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)
