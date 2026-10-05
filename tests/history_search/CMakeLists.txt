add_executable(history_search_tests history_search_tests.cpp ../../src/slic3r/GUI/HistorySearchStore.cpp)
target_include_directories(history_search_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(history_search_tests test_common libslic3r)
add_test(history_search_tests history_search_tests)
