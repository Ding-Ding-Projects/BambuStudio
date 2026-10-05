find_program(QUIET_PROMPTS_NODE_EXECUTABLE NAMES node node.exe)
if(QUIET_PROMPTS_NODE_EXECUTABLE)
    add_test(NAME quiet_prompt_source_contract
        COMMAND "${QUIET_PROMPTS_NODE_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/check-quiet-prompts.mjs")
else()
    message(STATUS "Quiet prompt source contract requires Node.js; run scripts/check-quiet-prompts.mjs separately")
endif()
