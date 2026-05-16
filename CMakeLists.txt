cmake_minimum_required(VERSION 3.20)
project(dcc C CXX) # CXX for llvm

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(DCC_BACKEND llvm CACHE STRING "Code generation backend option")
option(DCC_BACKEND_LINK_LLVM_DYLIB "Link against the LLVM dynamic library for llvm backend" ON)


add_compile_options(
    -g
    -fno-common
    -Wall
    -Wno-switch
    )

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(dcc_core STATIC src/tokenizer.c
                        src/parser.c
                        src/strings.c
                        src/type.c
                        src/hashmap.c
                        src/unicode.c
                        )

add_executable(dcc)
target_link_libraries(dcc PRIVATE dcc_core)
target_sources(dcc PRIVATE src/main.c
                        src/args.c
                        src/pathlib.c
                        )

# tools
add_executable(astrepr)
target_link_libraries(astrepr PRIVATE dcc_core)
target_sources(astrepr PRIVATE src/tools/astrepr.c)


include_directories(src/)

if(DCC_BACKEND STREQUAL "llvm")
    target_sources(dcc PRIVATE src/codegen/llvm.c)
    find_package(LLVM REQUIRED CONFIG)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
    include_directories(${LLVM_INCLUDE_DIRS})
    separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
    add_definitions(${LLVM_DEFINITIONS_LIST})

    if(DCC_BACKEND_LINK_LLVM_DYLIB)
        target_link_libraries(dcc PRIVATE LLVM)
    else()
        # https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project
        llvm_map_components_to_libnames(llvm_libs support core analysis native)
        target_link_libraries(dcc PRIVATE ${llvm_libs})
    endif()

else()
    message(FATAL_ERROR "Unsupported backend: ${DCC_BACKEND}")
endif()
