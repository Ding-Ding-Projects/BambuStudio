add_executable(home_assistant_tests
    home_assistant_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/HomeAssistantSharingService.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/HomeAssistantTransportPolicy.cpp
)
target_include_directories(home_assistant_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(home_assistant_tests PRIVATE cxx_std_17)
target_link_libraries(home_assistant_tests PRIVATE test_common boost_libs)
set_property(TARGET home_assistant_tests PROPERTY FOLDER "tests")
add_test(
    NAME home_assistant_tests
    COMMAND home_assistant_tests "${CATCH_EXTRA_ARGS}"
)

add_test(
    NAME home_assistant_http_security_contract
    COMMAND "${CMAKE_COMMAND}"
            "-DBAMBU_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/home_assistant_http_security_contract.cmake"
)

add_test(
    NAME home_assistant_ui_performance_contract
    COMMAND "${CMAKE_COMMAND}"
            "-DBAMBU_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/home_assistant_ui_performance_contract.cmake"
)

add_test(
    NAME home_assistant_amplification_contract
    COMMAND "${CMAKE_COMMAND}"
            "-DBAMBU_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/home_assistant_amplification_contract.cmake"
)

add_test(
    NAME home_assistant_service_fanout_contract
    COMMAND "${CMAKE_COMMAND}"
            "-DBAMBU_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/home_assistant_service_fanout_contract.cmake"
)

add_executable(home_assistant_sharing_probe
    home_assistant_sharing_probe.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/HomeAssistantSharingService.cpp
)
target_include_directories(home_assistant_sharing_probe PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(home_assistant_sharing_probe PRIVATE cxx_std_17)
target_link_libraries(home_assistant_sharing_probe PRIVATE boost_libs)
set_property(TARGET home_assistant_sharing_probe PROPERTY FOLDER "tests")
