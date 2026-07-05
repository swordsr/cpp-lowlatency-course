course_assignment(
    ID    m11a03
    LIB   src/pipeline.cpp
    TESTS tests/pipeline_test.cpp
)
# The whole stack: builder (m11a02 -> m11a01 -> m10a01 + m09a03),
# rings (m08a01), sockets (m09a01).
target_link_libraries(m11a03 PUBLIC m11a02 m08a01 m09a01 m06a01)

# The feed replayer tool (given, complete) — drives your pipeline live.
add_executable(m11a03_replayer tools/replayer.cpp)
target_link_libraries(m11a03_replayer PRIVATE m11a01)
