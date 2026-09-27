# Pure-model tests for the startup dim sum surprise. The header under test is
# wxWidgets-free, so this target needs only Catch2 and the bundled nlohmann
# json header from src/.
add_executable(dim_sum_tests
    dim_sum_tests_main.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/DimSumSurpriseModel.hpp
)
target_include_directories(dim_sum_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(dim_sum_tests PRIVATE cxx_std_17)
target_link_libraries(dim_sum_tests PRIVATE test_common)
set_property(TARGET dim_sum_tests PROPERTY FOLDER "tests")

add_test(NAME dim_sum_tests COMMAND dim_sum_tests "${CATCH_EXTRA_ARGS}")
