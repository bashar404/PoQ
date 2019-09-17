cmake_minimum_required(VERSION 3.14)
project(PoET_Code C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(PoET_Code
        POET++.c
        queue.h)

target_link_libraries(PoET_Code m)