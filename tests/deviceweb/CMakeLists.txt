add_executable(deviceweb_home_webview_failure_policy_tests
    home_webview_failure_policy_tests.cpp
)
target_include_directories(deviceweb_home_webview_failure_policy_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(deviceweb_home_webview_failure_policy_tests PRIVATE cxx_std_17)
target_link_libraries(deviceweb_home_webview_failure_policy_tests test_common)
set_property(TARGET deviceweb_home_webview_failure_policy_tests PROPERTY FOLDER "tests")
add_test(
    NAME deviceweb_home_webview_failure_policy_tests
    COMMAND deviceweb_home_webview_failure_policy_tests "${CATCH_EXTRA_ARGS}"
)

add_test(
    NAME deviceweb_home_webview_failure_contract
    COMMAND "${CMAKE_COMMAND}"
            "-DBAMBU_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/home_webview_failure_contract.cmake"
)
