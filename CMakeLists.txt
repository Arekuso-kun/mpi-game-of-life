cmake_minimum_required(VERSION 3.16)

project(mpi_game_of_life LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(life_core
    src/life.cpp
    src/pgm.cpp
)

target_include_directories(life_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(life_serial
    src/main_serial.cpp
)

target_link_libraries(life_serial
    PRIVATE
        life_core
)

add_executable(life_tests
    tests/test_life.cpp
)

target_link_libraries(life_tests
    PRIVATE
        life_core
)

enable_testing()
add_test(NAME life_tests COMMAND life_tests)
