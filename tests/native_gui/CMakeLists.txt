add_test(
    NAME native_gui_accessibility_contract
    COMMAND "${CMAKE_COMMAND}"
            "-DBAMBU_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/native_gui_accessibility_contract.cmake"
)

add_executable(fan_motion_math_test fan_motion_math.cpp)
target_compile_features(fan_motion_math_test PRIVATE cxx_std_17)
add_test(NAME fan_motion_math COMMAND fan_motion_math_test)
