#*******************************************************************************
#  Copyright (c) 2015, 2026 logi.cals GmbH, Frank Pagliughi <fpagliughi@mindspring.com> and others
#
#  All rights reserved. This program and the accompanying materials
#  are made available under the terms of the Eclipse Public License v2.0
#  and Eclipse Distribution License v1.0 which accompany this distribution.
#
#  The Eclipse Public License is available at
#     https://www.eclipse.org/legal/epl-2.0/
#  and the Eclipse Distribution License is available at
#    http://www.eclipse.org/org/documents/edl-v10.php.
#
#  Contributors:
#     Rainer Poisel - initial version
#     Genis Riera Perez - Add support for building debian package
#*******************************************************************************/

cmake_minimum_required(VERSION 3.12)

#创建一个叫gateway C的项目
project(gateway C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

#用main.c创建一个gateway的可执行程序
add_executable(gateway
    src/tcp_server.c
    src/udp_server.c
    src/message_queue.c
)

add_executable(tcp_client
    src/tcp_client.c
)

add_executable(udp_client
    test/udp_client.c
)

#使用linux的线程库
find_package(Threads REQUIRED)

#最终链接线程相关库
target_link_libraries(gateway
    PRIVATE Threads::Threads
)