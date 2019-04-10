cmake_minimum_required(VERSION 3.14)
project(untitled2 C)

set(CMAKE_C_STANDARD 11)

add_executable(edgc-dm main.c utils.c)
target_link_libraries(edgc-dm jansson)
target_link_libraries(edgc-dm curl)
target_link_libraries(edgc-dm ssl)
target_link_libraries(edgc-dm crypto)