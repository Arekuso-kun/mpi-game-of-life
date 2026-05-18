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
        NAME validation_serial_reference
        COMMAND $<TARGET_FILE:life_serial>
            --width 32
            --height 32
            --steps 20
            --pattern random
            --seed 42
            --density 0.25
            --snapshot-interval 20
            --output ${CMAKE_BINARY_DIR}/validation_serial
    )

    add_test(
        NAME validation_mpi_reference
        COMMAND ${MPIEXEC_EXECUTABLE}
            ${MPIEXEC_NUMPROC_FLAG} 4
            ${MPIEXEC_PREFLAGS}
            $<TARGET_FILE:life_mpi>
            ${MPIEXEC_POSTFLAGS}
            --width 32
            --height 32
            --steps 20
            --pattern random
            --seed 42
            --density 0.25
            --snapshot-interval 20
            --output ${CMAKE_BINARY_DIR}/validation_mpi
    )

    add_test(
        NAME validation_serial_vs_mpi
        COMMAND ${CMAKE_COMMAND} -E compare_files
            ${CMAKE_BINARY_DIR}/validation_serial/frame_000020.pgm
            ${CMAKE_BINARY_DIR}/validation_mpi/frame_000020.pgm
    )

    set_tests_properties(validation_mpi_reference PROPERTIES
        DEPENDS validation_serial_reference
    )
    set_tests_properties(validation_serial_vs_mpi PROPERTIES
        DEPENDS "validation_serial_reference;validation_mpi_reference"
    )
else()
    message(STATUS "MPI C++ compiler not found; skipping life_mpi target")
endif()
