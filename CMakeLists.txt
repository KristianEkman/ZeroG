cmake_minimum_required(VERSION 3.15)
project(zerog C)

# Use C11 standard to ensure native stdatomic.h support
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Configure output directories for all targets
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/builds)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/builds)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/builds)

# Include directories
include_directories(
    src
    src/movegen
    src/hashing
    src/uci
    src/search
    src/eval
    src/nn
    test
    test/movegen_tests
)

if(WIN32)
    # Include POSIX compatibility headers for Windows
    include_directories(win_compat)
    
    if(MSVC)
        # Enable C11 atomics support, target AVX2, and suppress CRT security warnings
        add_compile_options(/experimental:c11atomics /std:c11 /FImsvc_compat.h /arch:AVX2)
        add_compile_definitions(_CRT_SECURE_NO_WARNINGS __restrict__=__restrict)
    endif()
endif()

# Core sources (wildcard equivalents)
file(GLOB CORE_SRCS src/*.c)
file(GLOB MOVEGEN_SRCS src/movegen/*.c)
file(GLOB HASHING_SRCS src/hashing/*.c)
file(GLOB UCI_SRCS src/uci/*.c)
file(GLOB SEARCH_SRCS src/search/*.c)
file(GLOB EVAL_SRCS src/eval/*.c)
file(GLOB NN_SRCS src/nn/*.c)

set(LIB_SRCS
    ${CORE_SRCS}
    ${MOVEGEN_SRCS}
    ${HASHING_SRCS}
    ${UCI_SRCS}
    ${SEARCH_SRCS}
    ${EVAL_SRCS}
    ${NN_SRCS}
)
list(REMOVE_ITEM LIB_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")

# Library target for shared source files
add_library(zerog_lib STATIC ${LIB_SRCS})

# Find and link threads library on Unix systems
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(zerog_lib PRIVATE Threads::Threads)
endif()

# Main Engine Executable
add_executable(zerog src/main.c)
target_link_libraries(zerog zerog_lib)

# Benchmarks
add_executable(perft_bench test/perft_bench.c)
target_link_libraries(perft_bench zerog_lib)

add_executable(search_bench test/search_bench.c)
target_link_libraries(search_bench zerog_lib)

add_executable(nn_bench test/nn_bench.c)
target_link_libraries(nn_bench zerog_lib)

# Trainer tools
add_executable(nn_trainer src/nn/trainer/nn_trainer.c)
target_link_libraries(nn_trainer zerog_lib)

add_executable(epd_dedup src/nn/trainer/epd_dedup.c)
target_link_libraries(epd_dedup zerog_lib)

# Unit Test Runners
add_executable(test_runner test/test_boards.c test/unity.c)
target_link_libraries(test_runner zerog_lib)

add_executable(fen_test_runner test/fen_test.c test/unity.c)
target_link_libraries(fen_test_runner zerog_lib)

file(GLOB MOVEGEN_TEST_SRCS test/movegen_tests/*.c)
add_executable(movegen_test_runner ${MOVEGEN_TEST_SRCS} test/unity.c)
target_link_libraries(movegen_test_runner zerog_lib)

add_executable(eval_test_runner test/eval_test.c test/unity.c)
target_link_libraries(eval_test_runner zerog_lib)

add_executable(search_test_runner test/search_test.c test/unity.c)
target_link_libraries(search_test_runner zerog_lib)

add_executable(nn_test_runner test/nn_test.c test/unity.c)
target_link_libraries(nn_test_runner zerog_lib)

add_executable(tuning_test_runner test/tuning_test.c test/unity.c)
target_link_libraries(tuning_test_runner zerog_lib)

add_executable(uci_test_runner test/uci_test.c test/unity.c)
target_link_libraries(uci_test_runner zerog_lib)
