add_executable(bounded_regex_tests
    bounded_regex_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/Widgets/BoundedRegex.cpp
)

add_executable(bounded_regex_hanging_worker hanging_worker.cpp)
target_compile_features(bounded_regex_hanging_worker PRIVATE cxx_std_17)
set_property(TARGET bounded_regex_hanging_worker PROPERTY FOLDER "tests/helpers")

add_executable(bounded_regex_delayed_worker
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/Widgets/BoundedRegexWorkerMain.cpp
)
target_include_directories(bounded_regex_delayed_worker PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(bounded_regex_delayed_worker PRIVATE cxx_std_17)
target_compile_definitions(bounded_regex_delayed_worker PRIVATE
    BOUNDED_REGEX_TEST_STARTUP_DELAY_MS=300
    BOOST_REGEX_MAX_STATE_COUNT=1000000
)
target_link_libraries(bounded_regex_delayed_worker PRIVATE Boost::regex)
set_property(TARGET bounded_regex_delayed_worker PROPERTY FOLDER "tests/helpers")

add_executable(bounded_regex_eval_hanging_worker
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/Widgets/BoundedRegexWorkerMain.cpp
)
target_include_directories(bounded_regex_eval_hanging_worker PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(bounded_regex_eval_hanging_worker PRIVATE cxx_std_17)
target_compile_definitions(bounded_regex_eval_hanging_worker PRIVATE
    BOUNDED_REGEX_TEST_EVALUATION_DELAY_MS=1000
    BOOST_REGEX_MAX_STATE_COUNT=1000000
)
target_link_libraries(bounded_regex_eval_hanging_worker PRIVATE Boost::regex)
set_property(TARGET bounded_regex_eval_hanging_worker PROPERTY FOLDER "tests/helpers")

target_include_directories(bounded_regex_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(bounded_regex_tests PRIVATE cxx_std_17)
target_compile_definitions(bounded_regex_tests PRIVATE
    BOUNDED_REGEX_TEST_WORKER_PATH=R"\($<TARGET_FILE:BambuStudio_regex_worker>\)"
    BOUNDED_REGEX_HANGING_WORKER_PATH=R"\($<TARGET_FILE:bounded_regex_hanging_worker>\)"
    BOUNDED_REGEX_DELAYED_WORKER_PATH=R"\($<TARGET_FILE:bounded_regex_delayed_worker>\)"
    BOUNDED_REGEX_EVAL_HANGING_WORKER_PATH=R"\($<TARGET_FILE:bounded_regex_eval_hanging_worker>\)"
)
target_link_libraries(bounded_regex_tests test_common Boost::regex)
add_dependencies(bounded_regex_tests BambuStudio_regex_worker bounded_regex_hanging_worker
                 bounded_regex_delayed_worker bounded_regex_eval_hanging_worker)
set_property(TARGET bounded_regex_tests PROPERTY FOLDER "tests")

add_test(bounded_regex_tests bounded_regex_tests "${CATCH_EXTRA_ARGS}")
