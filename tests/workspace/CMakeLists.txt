add_executable(workspace_tests workspace_tests.cpp)
target_link_libraries(workspace_tests test_common libslic3r_workspace)
set_property(TARGET workspace_tests PROPERTY FOLDER "tests")
add_test(workspace_tests workspace_tests "${CATCH_EXTRA_ARGS}")
