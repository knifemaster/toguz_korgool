cmake_minimum_required(VERSION 3.10)

project(GoogleOAuth CXX)

set(CMAKE_CXX_STANDART 17)

add_executable(google_oauth src/main.cpp)

target_include_directories(google_oauth PRIVATE cpp-httplib)
