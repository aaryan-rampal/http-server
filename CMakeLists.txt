# Set minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Set project name
project(NetworkApp C)

# Set compile flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -g")

# Add executable for the server
add_executable(server server.c)

# Add executable for the client
add_executable(client client.c)

