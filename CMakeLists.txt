cmake_minimum_required(VERSION 3.10)

project(Toguz_korgool)

set(CMAKE_CXX_STANDART 23)
set(CMAKE_CXX_STANDART_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-g")

set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")

set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")



#set(LIB_DIR "${CMAKE_SOURCE_DIR}/lib")


add_executable(Toguz_korgool  ${SOURCE_DIR}/main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIR})

find_package(Boost REQUIRED COMPONENTS system date_time)


include_directories(${Boost_INCLUDE_DIRS})


target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})


