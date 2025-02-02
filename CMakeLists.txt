cmake_minimum_required(VERSION 3.10)

# Set project name and language (C++)
project(NetworkApp CXX)

# Set compile flags for C++
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -g")

# Add executable for the server
add_executable(server server.cpp)

add_executable(test-server test_server.cpp)

# Add executable for the client
add_executable(client client.cpp)
