if(NOT DEFINED SERIAL_EXE)
    message(FATAL_ERROR "SERIAL_EXE is required")
endif()

if(NOT DEFINED MPI_EXE)
    message(FATAL_ERROR "MPI_EXE is required")
endif()

if(NOT DEFINED MPIEXEC_EXECUTABLE)
    message(FATAL_ERROR "MPIEXEC_EXECUTABLE is required")
endif()

if(NOT DEFINED MPIEXEC_NUMPROC_FLAG)
    set(MPIEXEC_NUMPROC_FLAG "-n")
endif()

if(NOT DEFINED OUTPUT_DIR)
    set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/validation")
endif()

set(SERIAL_DIR "${OUTPUT_DIR}/serial")
set(MPI_DIR "${OUTPUT_DIR}/mpi")
set(FRAME_NAME "frame_000020.pgm")

file(REMOVE_RECURSE "${SERIAL_DIR}" "${MPI_DIR}")

if(DEFINED LIFE_TESTS_EXE)
    execute_process(
        COMMAND "${LIFE_TESTS_EXE}"
        RESULT_VARIABLE life_tests_result
    )

    if(NOT life_tests_result EQUAL 0)
        message(FATAL_ERROR "life_tests failed with exit code ${life_tests_result}")
    endif()
endif()

execute_process(
    COMMAND "${SERIAL_EXE}"
        --width 32
        --height 32
        --steps 20
        --pattern random
        --seed 42
        --density 0.25
        --snapshot-interval 20
        --output "${SERIAL_DIR}"
    RESULT_VARIABLE serial_result
)

if(NOT serial_result EQUAL 0)
    message(FATAL_ERROR "Serial validation run failed with exit code ${serial_result}")
endif()

execute_process(
    COMMAND "${MPIEXEC_EXECUTABLE}"
        "${MPIEXEC_NUMPROC_FLAG}" 4
        "${MPI_EXE}"
        --width 32
        --height 32
        --steps 20
        --pattern random
        --seed 42
        --density 0.25
        --snapshot-interval 20
        --output "${MPI_DIR}"
    RESULT_VARIABLE mpi_result
)

if(NOT mpi_result EQUAL 0)
    message(FATAL_ERROR "MPI validation run failed with exit code ${mpi_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${SERIAL_DIR}/${FRAME_NAME}"
        "${MPI_DIR}/${FRAME_NAME}"
    RESULT_VARIABLE compare_result
)

if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "Serial and MPI validation frames differ")
endif()

message(STATUS "Serial and MPI validation frames are identical")
