cmake_minimum_required(VERSION 3.10)
project(PoET_Code CXX C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

include_directories(.)

set(DEBUG)
set(SGX_HW ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
find_package(SGX REQUIRED)

add_executable(poet_main
        POET++.c socket_t.c queue_t.c general_structs.c)
target_link_libraries(poet_main m pthread)

# --------------- CLIENT ---------------------

set(EDL_SEARCH_PATHS poet_client/enclave)
set(E_SRCS poet_client/enclave/enclave.c)
set(LDS poet_client/enclave/enclave.lds)
add_enclave_library(enclave SRCS ${E_SRCS} EDL poet_client/enclave/enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS} LDSCRIPT ${LDS})
enclave_sign(enclave KEY poet_client/enclave/enclave_private.pem CONFIG poet_client/enclave/enclave.config.xml)
set(SRCS poet_client/poet_client.cpp poet_client/enclave_helper.c socket_t.c queue_t.c
          general_structs.c json-parser/json.c poet_functions.c)
add_untrusted_executable(client SRCS ${SRCS} EDL poet_client/enclave/enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS})
add_dependencies(client enclave-sign)
#add_untrusted_library(poet_client
#        STATIC | SHARED | MODULE
#        [USE_PREFIX]
#        SRCS src1 src2 ...
#        EDL edl_file1 edl_file2 ...
#        EDL_SEARCH_PATHS path1 path2 ...)
#target_link_libraries(poet_client m pthread)

#add_executable(poet_client
#        poet_client/poet_client.cpp socket_t.c queue_t.c
#        general_structs.c json-parser/json.c  poet_client/enclave/enclave.c)
#target_link_libraries(poet_client m pthread)

# --------------- SERVER ---------------------

add_executable(poet_server
        poet_server.cpp socket_t.c queue_t.c
        general_structs.c json-parser/json.c poet_server_functions.c poet_functions.c)
target_link_libraries(poet_server m pthread)
