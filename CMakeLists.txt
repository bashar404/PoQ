cmake_minimum_required(VERSION 3.10)
project(PoET_Code CXX C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

include_directories(.)

#add_compile_definitions(DEBUG)

add_executable(poet_main
        POET++.c queue_t.h socket_t.h socket_t.c queue_t.c
        poet_node_t.h general_structs.h general_structs.c)
target_link_libraries(poet_main m pthread)

add_executable(poet_client
        poet_client.cpp socket_t.c queue_t.c
        enclave.h poet_node_t.h general_structs.h general_structs.c json-parser/json.h json-parser/json.c poet_functions.h)
target_link_libraries(poet_client m pthread)

add_executable(poet_server
        poet_server.cpp socket_t.c queue_t.c
        poet_node_t.h general_structs.h general_structs.c json-parser/json.h json-parser/json.c poet_server_functions.h poet_server_functions.c poet_functions.h poet_functions.c)
target_link_libraries(poet_server m pthread)
