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

set(DCC_CORE_SOURCES src/tokenizer.c
                     src/parser.c
                     src/strings.c
                     src/type.c
                     src/hashmap.c
                     src/unicode.c
                     src/ptrarray.c
                     )

set(DCC_MAIN_SOURCES src/main.c
                     src/args.c
                     src/pathlib.c
                     )

if(DCC_BACKEND STREQUAL "llvm")
    list(APPEND DCC_MAIN_SOURCES src/codegen/llvm.c)
else()
    message(FATAL_ERROR "Unsupported backend: ${DCC_BACKEND}")
endif()

set(DCC_STAGE2_ALL_SOURCES ${DCC_CORE_SOURCES} ${DCC_MAIN_SOURCES})

#
## Stage 1: Build the core library, the tools and the main executable
#
add_library(dcc_core STATIC ${DCC_CORE_SOURCES})
target_include_directories(dcc_core PUBLIC src/)

add_executable(dcc ${DCC_MAIN_SOURCES})
target_link_libraries(dcc PRIVATE dcc_core)

# tools
add_executable(astrepr src/tools/astrepr.c)
target_link_libraries(astrepr PRIVATE dcc_core)

if(DCC_BACKEND STREQUAL "llvm")
    find_package(LLVM REQUIRED CONFIG)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
    target_include_directories(dcc PRIVATE ${LLVM_INCLUDE_DIRS})
    separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
    target_compile_definitions(dcc PRIVATE ${LLVM_DEFINITIONS_LIST})

    if(DCC_BACKEND_LINK_LLVM_DYLIB)
        set(DCC_LLVM_LIBS LLVM)
    else()
        # https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project
        llvm_map_components_to_libnames(llvm_libs support core analysis native)
        set(DCC_LLVM_LIBS ${llvm_libs})
    endif()
    target_link_libraries(dcc PRIVATE ${DCC_LLVM_LIBS})
endif()

add_subdirectory(stage2)