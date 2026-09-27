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

add_executable(farm_device_policy_tests farm_device_policy_tests.cpp)
target_compile_features(farm_device_policy_tests PRIVATE cxx_std_17)
find_package(Threads REQUIRED)
target_link_libraries(farm_device_policy_tests PRIVATE Threads::Threads)
set_property(TARGET farm_device_policy_tests PROPERTY FOLDER "tests")
add_test(NAME farm_device_policy_tests COMMAND farm_device_policy_tests)
