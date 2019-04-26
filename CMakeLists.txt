cmake_minimum_required(VERSION 3.14)
project(edgc-dm C)

set(CMAKE_C_STANDARD 11)

add_executable(edgc-dm main.c http.c http.h utils.c utils.h monitor.c monitor.h report.c report.h task.c task.h)

target_link_libraries(edgc-dm jansson)
target_link_libraries(edgc-dm curl)
target_link_libraries(edgc-dm ssl)
target_link_libraries(edgc-dm crypto)
target_link_libraries(edgc-dm pthread)
target_link_libraries(edgc-dm m)