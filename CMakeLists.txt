cmake_minimum_required(VERSION 3.12)

project(gateway C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Linux 线程库
find_package(Threads REQUIRED)


# =========================
# Gateway
# =========================
#
# main.c
#     ↓
# TCP Server
# UDP Server
# Worker
#     ↓
# Message Queue
#
add_executable(gateway
    src/main.c
    src/message_queue.c
    src/tcp_server.c
    src/udp_server.c
)

target_link_libraries(gateway
    PRIVATE Threads::Threads
)


# =========================
# TCP Client 测试程序
# =========================
add_executable(tcp_client
    src/tcp_client.c
)


# =========================
# UDP Client 测试程序
# =========================
add_executable(udp_client
    test/udp_client.c
)

# epoll demo
add_executable(epoll_demo
    test/epoll_demo.c
)