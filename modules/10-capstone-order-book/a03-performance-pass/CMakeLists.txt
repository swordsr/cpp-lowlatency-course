course_assignment(
    ID    m10a03
    LIB   src/fast_engine.cpp
    TESTS tests/fast_engine_test.cpp
    BENCH bench/fast_engine_bench.cpp
)
# Differential tests replay against your a02 (which carries your a01);
# the bench measures with your m06a01 histogram.
target_link_libraries(m10a03 PUBLIC m10a02)
target_link_libraries(m10a03_bench PRIVATE m06a01)
