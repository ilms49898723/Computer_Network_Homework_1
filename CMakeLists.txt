cmake_minimum_required(VERSION 3.5)
project(Network_Programming_Homework_1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    client.cpp
    server.cpp)

add_executable(server server.cpp)
add_executable(client client.cpp)
