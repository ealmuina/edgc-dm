cmake_minimum_required(VERSION 3.5)
project(edgc-dm C)

set(CMAKE_C_STANDARD 11)
set(LIB_PATHS /usr/local $ENV{HOME}/.local/lib)

add_executable(edgc-dm
        main.c
        http.c include/http.h
        utils.c include/utils.h
        monitor.c include/monitor.h
        report.c include/report.h
        task.c include/task.h)

target_include_directories(edgc-dm PRIVATE $ENV{HOME}/.local/include)
find_library(LJANSSON jansson PATHS ${LIB_PATHS})
find_library(LCURL curl PATHS ${LIB_PATHS})

target_link_libraries(edgc-dm ${LJANSSON})
target_link_libraries(edgc-dm ${LCURL})
target_link_libraries(edgc-dm ssl)
target_link_libraries(edgc-dm crypto)
target_link_libraries(edgc-dm pthread)
target_link_libraries(edgc-dm m)