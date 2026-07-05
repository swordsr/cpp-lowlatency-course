course_assignment(
    ID    m11a02
    TESTS tests/book_builder_test.cpp
)
# Feed session (m11a01) mirrored into your order book (m10a01).
target_link_libraries(m11a02 INTERFACE m11a01 m10a01)
