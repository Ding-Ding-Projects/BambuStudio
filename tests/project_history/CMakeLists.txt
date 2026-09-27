add_executable(project_history_tests
    project_history_tests.cpp
)

target_link_libraries(project_history_tests
    test_common
    libslic3r_project_history
)
set_property(TARGET project_history_tests PROPERTY FOLDER "tests")

add_test(project_history_tests project_history_tests "${CATCH_EXTRA_ARGS}")
