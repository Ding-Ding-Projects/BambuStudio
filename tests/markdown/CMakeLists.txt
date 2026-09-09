# Renderer tests for src/libslic3r/Markdown.{hpp,cpp}: the ONE Markdown
# renderer behind the in-app documentation browser. The renderer is wx-free
# and dependency-free, so the source is compiled straight into the test
# rather than pulling the whole libslic3r target in.
add_executable(markdown_tests
    markdown_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/libslic3r/Markdown.cpp
)

target_include_directories(markdown_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(markdown_tests test_common)
set_property(TARGET markdown_tests PROPERTY FOLDER "tests")

add_test(markdown_tests markdown_tests "${CATCH_EXTRA_ARGS}")
