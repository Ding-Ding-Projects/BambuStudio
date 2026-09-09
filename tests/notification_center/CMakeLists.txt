add_executable(notification_center_tests
    notification_center_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/NotificationHistory.cpp
)

# NotificationHistory is a pure C++17 model (nlohmann::json only, no wxWidgets
# or ImGui), so this target links nothing beyond the Catch2 common library.
target_include_directories(notification_center_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(notification_center_tests test_common)
set_property(TARGET notification_center_tests PROPERTY FOLDER "tests")

add_test(notification_center_tests notification_center_tests "${CATCH_EXTRA_ARGS}")
