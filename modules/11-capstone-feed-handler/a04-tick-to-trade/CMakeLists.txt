course_assignment(
    ID    m11a04
    TESTS tests/tick_to_trade_test.cpp
    BENCH bench/tick_to_trade_bench.cpp
)
# Measurement of the a03 pipeline (which carries the whole course).
target_link_libraries(m11a04 INTERFACE m11a03)
target_link_libraries(m11a04_bench PRIVATE m11a03)
