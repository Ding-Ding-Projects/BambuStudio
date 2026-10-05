add_test(
    NAME sidebar_filament_combo_bounds_contract
    COMMAND "${CMAKE_COMMAND}"
            "-DBAMBU_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/sidebar_filament_combo_bounds_contract.cmake"
)
# Extract actual production bodies so these behavior tests cannot silently test a
# separately reimplemented selection algorithm.
file(READ "${CMAKE_SOURCE_DIR}/src/slic3r/GUI/Widgets/DropDown.cpp" dropdown_source)
string(FIND "${dropdown_source}" "void DropDown::Invalidate(" invalidate_start)
string(SUBSTRING "${dropdown_source}" ${invalidate_start} -1 invalidate_tail)
string(FIND "${invalidate_tail}" "\n}" invalidate_end)
math(EXPR invalidate_length "${invalidate_end} + 2")
string(SUBSTRING "${invalidate_tail}" 0 ${invalidate_length} invalidate_body)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/dropdown_invalidate.inc" "${invalidate_body}\n")
string(FIND "${dropdown_source}" "void DropDown::sendDropDownEvent()" dispatch_start)
string(SUBSTRING "${dropdown_source}" ${dispatch_start} -1 dispatch_tail)
string(FIND "${dispatch_tail}" "\n}" dispatch_end)
math(EXPR dispatch_length "${dispatch_end} + 2")
string(SUBSTRING "${dispatch_tail}" 0 ${dispatch_length} dispatch_body)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/dropdown_dispatch.inc" "${dispatch_body}\n")
file(READ "${CMAKE_SOURCE_DIR}/src/slic3r/GUI/Widgets/ComboBox.cpp" combo_source)
string(FIND "${combo_source}" "        const int index = e.GetInt();" adapter_start)
string(SUBSTRING "${combo_source}" ${adapter_start} -1 adapter_tail)
string(FIND "${adapter_tail}" "\n    });" adapter_end)
string(SUBSTRING "${adapter_tail}" 0 ${adapter_end} adapter_body)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/combo_dispatch.inc" "${adapter_body}\n")
add_executable(dropdown_lifecycle_tests dropdown_lifecycle_tests.cpp)
target_include_directories(dropdown_lifecycle_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(dropdown_lifecycle_tests test_common)
set_property(TARGET dropdown_lifecycle_tests PROPERTY FOLDER "tests")
add_test(NAME dropdown_lifecycle_tests COMMAND dropdown_lifecycle_tests "${CATCH_EXTRA_ARGS}")
