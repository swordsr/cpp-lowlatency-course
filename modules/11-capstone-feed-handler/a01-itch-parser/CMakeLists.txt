course_assignment(
    ID    m11a01
    TESTS tests/feed_test.cpp
)
# FeedSession decodes with YOUR m09a03 ByteReader.
target_link_libraries(m11a01 INTERFACE m09a03)
