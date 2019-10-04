cmake_minimum_required(VERSION 3.14)
project(PoET_Code C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

#add_compile_definitions(NDEBUG)

add_executable(poet_main
        POET++.c queue_t.h socket_t.h socket_t.c queue_t.c
        poet_node_t.h general_structs.h general_structs.c)
target_link_libraries(poet_main m pthread)

add_executable(poet_client
        poet_client.c socket_t.c queue_t.c
        enclave.h poet_node_t.h general_structs.h general_structs.c)
target_link_libraries(poet_client m pthread)

add_executable(poet_server
        poet_server.c socket_t.c queue_t.c
        poet_node_t.h general_structs.h general_structs.c)
target_link_libraries(poet_server m pthread)
