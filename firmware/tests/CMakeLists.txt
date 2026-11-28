# Host-side unit tests for the hardware-free modules in src/lib (D37).
# Each test is a small executable; CTest treats a non-zero exit as a failure.

#
# VECTOR_DIR points the tests at the Python model's exported vectors (D13), so
# the C code is checked against the same files CI regenerates.

set(VECTOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../sim/vectors)
cmake_path(NORMAL_PATH VECTOR_DIR)

foreach(t ringbuf crc32 nco dac_map)
    add_executable(test_${t} test_${t}.c)
    target_link_libraries(test_${t} PRIVATE lockin_lib)
    target_compile_options(test_${t} PRIVATE ${LOCKIN_WARNINGS})
    target_compile_definitions(test_${t} PRIVATE VECTOR_DIR="${VECTOR_DIR}")
    add_test(NAME ${t} COMMAND test_${t})
endforeach()
