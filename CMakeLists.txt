cmake_minimum_required(VERSION 3.10)
project(dcc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_compile_options(
    -g
    -fno-common
    -Wall
    -Wno-switch
)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SRCS src/tokenizer.c
        src/parser.c
        src/strings.c
        src/type.c
        src/hashmap.c
        src/astrepr.c
        src/main.c)


include_directories(src/)

add_executable(dcc ${SRCS})