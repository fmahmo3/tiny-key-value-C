cmake_minimum_required(VERSION 3.16)

project(tinykv C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Library: tinykv (the core key–value store)
add_library(tinykv
    src/tinykv.c
)

target_include_directories(tinykv PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# CLI executable (for now, just src/main.c)
add_executable(tinykv_cli
    src/main.c
)

target_link_libraries(tinykv_cli PRIVATE tinykv)

# (Optional) Test target placeholder
enable_testing()

add_executable(tinykv_basic_test
    tests/basic_test.c
)

target_link_libraries(tinykv_basic_test PRIVATE tinykv)

add_test(NAME basic COMMAND tinykv_basic_test)
