cmake_minimum_required(VERSION 3.10)

project(ChatRoom01)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(server
    src/server_main.cpp
    src/tcp_server.cpp
)

add_executable(client
    src/client_main.cpp
    src/tcp_client.cpp
)