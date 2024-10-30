# Set minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Set project name
project(NetworkApp)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set compile flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -g")

# Add executable for the server
add_executable(server server.cpp)

# Add executable for the client
add_executable(client client.cpp)
