# course_assignment() — declares the standard targets for one assignment.
#
#   course_assignment(
#       ID    m02a03                      # module 02, assignment 03
#       LIB   src/unique_ptr.cpp          # omit for header-only assignments
#       TESTS tests/unique_ptr_test.cpp
#       BENCH bench/unique_ptr_bench.cpp  # optional
#   )
#
# Produces:
#   <ID>        library with include/ on its interface (INTERFACE lib if no LIB sources)
#   <ID>_tests  GoogleTest binary; cases registered with ctest as "<ID>.<Suite>.<Case>"
#               so `ctest -R m02a03` runs exactly this assignment
#   <ID>_bench  Google Benchmark binary (only if BENCH given); run manually
function(course_assignment)
    cmake_parse_arguments(ARG "" "ID" "LIB;TESTS;BENCH" ${ARGN})
    if(NOT ARG_ID)
        message(FATAL_ERROR "course_assignment: ID is required")
    endif()

    if(ARG_LIB)
        add_library(${ARG_ID} STATIC ${ARG_LIB})
        target_include_directories(${ARG_ID} PUBLIC include)
        target_compile_options(${ARG_ID} PRIVATE -Wall -Wextra -Wpedantic)
    else()
        add_library(${ARG_ID} INTERFACE)
        target_include_directories(${ARG_ID} INTERFACE include)
    endif()

    if(ARG_TESTS)
        add_executable(${ARG_ID}_tests ${ARG_TESTS})
        target_link_libraries(${ARG_ID}_tests PRIVATE ${ARG_ID} GTest::gtest_main)
        target_compile_options(${ARG_ID}_tests PRIVATE -Wall -Wextra)
        # TIMEOUT: a deadlocked test (hello, concurrency modules) fails
        # after 60s instead of hanging ctest forever.
        gtest_discover_tests(${ARG_ID}_tests
            TEST_PREFIX "${ARG_ID}."
            DISCOVERY_TIMEOUT 30
            PROPERTIES LABELS "${ARG_ID}" TIMEOUT 60)
    endif()

    if(ARG_BENCH)
        add_executable(${ARG_ID}_bench ${ARG_BENCH})
        target_link_libraries(${ARG_ID}_bench PRIVATE ${ARG_ID} benchmark::benchmark_main)
        target_compile_options(${ARG_ID}_bench PRIVATE -Wall -Wextra)
    endif()
endfunction()
