cmake_minimum_required(VERSION 3.10)
project(PoET_Code CXX C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

include_directories(.)

set(WITH_SGX OFF)
set(DEBUG 1)
set(SGX_HW ON)
add_definitions(-DNO_RA)

add_definitions(-g)

if (DEBUG)
    if (DEBUG STREQUAL "2")
        add_definitions(-DDEBUG)
        set(SGX_MODE_DEBUG 2)
    endif()

    if (DEBUG STREQUAL "1")
        add_definitions(-DDEBUG1)
        set(SGX_MODE_DEBUG 1)
    endif()

    if (DEBUG STREQUAL "0")
        add_definitions(-DWARNING)
        set(SGX_MODE_DEBUG 0)
    endif()

    set(SGX_MODE Debug)
else()
    add_definitions(-DNDEBUG)
    set(SGX_MODE PreRelease)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
if (WITH_SGX)
    find_package(SGX REQUIRED)
else()
    add_definitions(-DNO_SGX)
endif()

add_executable(poet_main
        POET++.cpp socket_t.c queue_t.c poet_shared_functions.cpp general_structs.cpp
        json-parser/json.c JSON-c/JSON_checker.c JSON-c/utf8_decode.c JSON-c/utf8_to_utf16.c)
target_link_libraries(poet_main m pthread)

add_executable(poet_test
        poet_methods_test.cpp
        socket_t.c queue_t.c poet_shared_functions.cpp general_structs.cpp poet_shared_functions.cpp
        json-parser/json.c JSON-c/JSON_checker.c JSON-c/utf8_decode.c JSON-c/utf8_to_utf16.c)
target_link_libraries(poet_test m pthread)

# --------------- CLIENT ---------------------

set(EDL_SEARCH_PATHS poet_client/enclave)
set(E_SRCS poet_client/enclave/enclave.c)
set(LDS poet_client/enclave/enclave.lds)
set(GENERAL_SRCS poet_client/poet_client.cpp socket_t.c queue_t.c
        poet_shared_functions.cpp general_structs.cpp json-parser/json.c
        JSON-c/JSON_checker.c JSON-c/utf8_decode.c JSON-c/utf8_to_utf16.c)
set(SRCS ${GENERAL_SRCS} poet_client/enclave_helper.c)

if (WITH_SGX)
    add_enclave_library(enclave SRCS ${E_SRCS} EDL poet_client/enclave/enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS} LDSCRIPT ${LDS})
    enclave_sign(enclave KEY poet_client/enclave/enclave_private.pem CONFIG poet_client/enclave/enclave.config.xml)
    add_untrusted_executable(client SRCS ${SRCS} EDL poet_client/enclave/enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS})
    add_dependencies(client enclave-sign)
else()
    add_executable(client ${GENERAL_SRCS})
    target_link_libraries(client m pthread)
endif ()

# --------------- SERVER ---------------------

add_executable(poet_server
        poet_server.cpp socket_t.c queue_t.c
        poet_shared_functions.cpp general_structs.cpp json-parser/json.c poet_server_functions.cpp
        JSON-c/JSON_checker.c JSON-c/utf8_decode.c JSON-c/utf8_to_utf16.c)
target_link_libraries(poet_server m pthread)
