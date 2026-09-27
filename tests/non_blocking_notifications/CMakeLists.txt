add_test(
    NAME non_decision_modal_paths_contract
    COMMAND "${CMAKE_COMMAND}"
            "-DBAMBU_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/non_decision_modal_paths_contract.cmake"
)
