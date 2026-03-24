cmake_minimum_required(VERSION 3.10)

project(ChatRoom01)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

find_package(Threads REQUIRED)

add_executable(server
    src/server_main.cpp
    src/tcp_server.cpp
)
target_link_libraries(server PRIVATE Threads::Threads)

add_executable(client
    src/client_main.cpp
    src/tcp_client.cpp
)
target_link_libraries(client PRIVATE Threads::Threads)

add_executable(web_bridge
    src/web_bridge_main.cpp
    src/web_bridge.cpp
)
target_link_libraries(web_bridge PRIVATE Threads::Threads)
