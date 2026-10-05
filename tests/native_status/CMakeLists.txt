cmake_minimum_required(VERSION 3.16)
project(native_status_tests LANGUAGES CXX)
set(STATUS_HUB_CLIENT_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(STATUS_HUB_CLIENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(../../libs/status-hub-client/cpp status_hub_client)
add_executable(native_status_tests
    status_service_test.cpp
    ../../src/libslic3r/StatusHub/StatusHubService.cpp)
target_compile_features(native_status_tests PRIVATE cxx_std_17)
target_link_libraries(native_status_tests PRIVATE status_hub_client)
target_include_directories(native_status_tests PRIVATE ../../src)
enable_testing()
add_test(NAME native_status_service COMMAND native_status_tests)
