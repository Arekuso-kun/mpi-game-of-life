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
set(VALIDATION_WIDTH 32)
set(VALIDATION_HEIGHT 32)
set(VALIDATION_STEPS 100)
set(VALIDATION_PATTERN random)
set(VALIDATION_SEED 42)
set(VALIDATION_DENSITY 0.25)
set(VALIDATION_PROCESSES 4)
string(REPEAT "0" 6 step_padding)
string(LENGTH "${VALIDATION_STEPS}" step_length)
math(EXPR padding_length "6 - ${step_length}")
if(padding_length GREATER 0)
    string(SUBSTRING "${step_padding}" 0 ${padding_length} step_prefix)
else()
    set(step_prefix "")
endif()
set(FRAME_NAME "frame_${step_prefix}${VALIDATION_STEPS}.pgm")

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
        --width ${VALIDATION_WIDTH}
        --height ${VALIDATION_HEIGHT}
        --steps ${VALIDATION_STEPS}
        --pattern ${VALIDATION_PATTERN}
        --seed ${VALIDATION_SEED}
        --density ${VALIDATION_DENSITY}
        --snapshot-interval ${VALIDATION_STEPS}
        --output "${SERIAL_DIR}"
    RESULT_VARIABLE serial_result
)

if(NOT serial_result EQUAL 0)
    message(FATAL_ERROR "Serial validation run failed with exit code ${serial_result}")
endif()

execute_process(
    COMMAND "${MPIEXEC_EXECUTABLE}"
        "${MPIEXEC_NUMPROC_FLAG}" ${VALIDATION_PROCESSES}
        "${MPI_EXE}"
        --width ${VALIDATION_WIDTH}
        --height ${VALIDATION_HEIGHT}
        --steps ${VALIDATION_STEPS}
        --pattern ${VALIDATION_PATTERN}
        --seed ${VALIDATION_SEED}
        --density ${VALIDATION_DENSITY}
        --snapshot-interval ${VALIDATION_STEPS}
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
