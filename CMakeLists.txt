cmake_minimum_required(VERSION 3.10)
project(edgc-dm C)

set(CMAKE_C_STANDARD 11)

add_executable(edgc-dm
        main.c
        http.c include/http.h
        utils.c include/utils.h
        monitor.c include/monitor.h
        report.c include/report.h
        task.c include/task.h)

target_link_libraries(edgc-dm jansson)
target_link_libraries(edgc-dm curl)
target_link_libraries(edgc-dm ssl)
target_link_libraries(edgc-dm crypto)
target_link_libraries(edgc-dm pthread)
target_link_libraries(edgc-dm m)