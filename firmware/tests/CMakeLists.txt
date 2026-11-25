# Host-side unit tests for the hardware-free modules in src/lib (D37).
# Each test is a small executable; CTest treats a non-zero exit as a failure.

foreach(t ringbuf crc32)
    add_executable(test_${t} test_${t}.c)
    target_link_libraries(test_${t} PRIVATE lockin_lib)
    target_compile_options(test_${t} PRIVATE ${LOCKIN_WARNINGS})
    add_test(NAME ${t} COMMAND test_${t})
endforeach()
