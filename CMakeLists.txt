cmake_minimum_required(VERSION 3.14)
project(PoET_Code C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(poet_main
        POET++.c queue_t.h socket_t.h socket_t.c queue_t.c
)
target_link_libraries(poet_main m)

add_executable(poet_client
        poet_client.c socket_t.c queue_t.c
        enclave.h)
target_link_libraries(poet_client m)

add_executable(poet_server
        poet_server.c socket_t.c queue_t.c
)
target_link_libraries(poet_server m)
