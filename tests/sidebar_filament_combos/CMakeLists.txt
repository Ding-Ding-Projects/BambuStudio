add_test(
    NAME sidebar_filament_combo_bounds_contract
    COMMAND "${CMAKE_COMMAND}"
            "-DBAMBU_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/sidebar_filament_combo_bounds_contract.cmake"
)
