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

find_package(MPI QUIET)

if(MPI_CXX_FOUND)
    add_executable(life_mpi
        src/main_mpi.cpp
    )

    target_link_libraries(life_mpi
        PRIVATE
            life_core
            MPI::MPI_CXX
    )

    add_test(
        NAME validation_suite
        COMMAND ${CMAKE_COMMAND}
            -D LIFE_TESTS_EXE=$<TARGET_FILE:life_tests>
            -D SERIAL_EXE=$<TARGET_FILE:life_serial>
            -D MPI_EXE=$<TARGET_FILE:life_mpi>
            -D MPIEXEC_EXECUTABLE=${MPIEXEC_EXECUTABLE}
            -D MPIEXEC_NUMPROC_FLAG=${MPIEXEC_NUMPROC_FLAG}
            -D OUTPUT_DIR=${CMAKE_BINARY_DIR}/validation
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ValidateSerialVsMpi.cmake
    )
else()
    add_test(NAME life_tests COMMAND life_tests)
    message(STATUS "MPI C++ compiler not found; skipping life_mpi target")
endif()
