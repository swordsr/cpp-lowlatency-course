course_assignment(
    ID    m10a02
    LIB   src/matching_engine.cpp
    TESTS tests/matching_engine_test.cpp
)
# The engine is built ON your a01 book — one capstone, staged.
target_link_libraries(m10a02 PUBLIC m10a01)
