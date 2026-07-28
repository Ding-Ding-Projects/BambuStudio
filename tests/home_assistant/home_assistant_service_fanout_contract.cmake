if(NOT DEFINED BAMBU_SOURCE_DIR)
    message(FATAL_ERROR "BAMBU_SOURCE_DIR is required")
endif()

set(HOME_ASSISTANT_SOURCE
    "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/HomeAssistant.cpp")
set(HOME_ASSISTANT_HEADER
    "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/HomeAssistant.hpp")
set(EXECUTOR_HEADER
    "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/HomeAssistantTaskExecutor.hpp")
set(SMART_HOME_SOURCE
    "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/SmartHomeDialog.cpp")
set(GUI_APP_SOURCE
    "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/GUI_App.cpp")
set(TEST_SOURCE
    "${BAMBU_SOURCE_DIR}/tests/home_assistant/home_assistant_tests.cpp")
foreach(REQUIRED_FILE IN ITEMS
        "${HOME_ASSISTANT_SOURCE}"
        "${HOME_ASSISTANT_HEADER}"
        "${EXECUTOR_HEADER}"
        "${SMART_HOME_SOURCE}"
        "${GUI_APP_SOURCE}"
        "${TEST_SOURCE}")
    if(NOT EXISTS "${REQUIRED_FILE}")
        message(FATAL_ERROR "Missing Home Assistant fan-out source: ${REQUIRED_FILE}")
    endif()
endforeach()

file(READ "${HOME_ASSISTANT_SOURCE}" HOME_ASSISTANT_TEXT)
file(READ "${HOME_ASSISTANT_HEADER}" HOME_ASSISTANT_HEADER_TEXT)
file(READ "${EXECUTOR_HEADER}" EXECUTOR_TEXT)
file(READ "${SMART_HOME_SOURCE}" SMART_HOME_TEXT)
file(READ "${GUI_APP_SOURCE}" GUI_APP_TEXT)
file(READ "${TEST_SOURCE}" TEST_TEXT)

foreach(REQUIRED_FRAGMENT IN ITEMS
        "constexpr std::size_t kServiceWorkerCount      = 4"
        "constexpr std::size_t kMaxQueuedServiceCalls   = 64"
        "constexpr std::size_t kMaxConfiguredEntities   = 32"
        "constexpr std::size_t kMaxPrinterHandoverCount = 32"
        "constexpr std::size_t kPrinterImportWorkerCount = 4"
        "constexpr std::size_t kMaxPendingLightTargets  = 4"
        "constexpr std::size_t kMaxStatesResponseBytes  = 4 * 1024 * 1024"
        "Execution::BoundedTaskExecutor"
        "Execution::LightAlertTransaction"
        "runtime_state().light_executor()"
        "runtime_state().query_executor()"
        "runtime_state().printer_executor()"
        "std::launch::async"
        "offset += kPrinterImportWorkerCount"
        "Accept-Encoding"
        "identity"
        "submit_with_discard"
        "Execution::valid_entity_domains"
        "Execution::bounded_semicolon_config_values"
        "post_printer_import_completion"
        "if (!done || s_shutdown_requested.load() || wxTheApp == nullptr)"
        "EntityFetchErrorCode::ShuttingDown"
        "EntityFetchErrorCode::NotConfigured"
        "EntityFetchErrorCode::InsecureTransport"
        "EntityFetchErrorCode::QueueFull"
        "EntityFetchErrorCode::RequestSetupFailed"
        "EntityFetchErrorCode::TransportError"
        "EntityFetchErrorCode::HttpStatus"
        "EntityFetchErrorCode::InvalidResponse"
        "EntityFetchErrorCode::ResponseTooLarge"
        ".on_progress("
        "cancel = !allow_during_shutdown && cancel_requested.load()"
        "if (s_shutdown_requested.load())"
        "void shutdown() noexcept")
    string(FIND "${HOME_ASSISTANT_TEXT}" "${REQUIRED_FRAGMENT}" FRAGMENT_OFFSET)
    if(FRAGMENT_OFFSET EQUAL -1)
        message(FATAL_ERROR
            "Missing Home Assistant runtime fan-out bound: ${REQUIRED_FRAGMENT}")
    endif()
endforeach()

foreach(REQUIRED_ENTITY_API_FRAGMENT IN ITEMS
        "enum class EntityFetchErrorCode"
        "struct EntityQuery"
        "struct EntityFetchResult"
        "using EntityFetchCallback"
        "void list_entities(EntityQuery query, EntityFetchCallback done)")
    string(FIND "${HOME_ASSISTANT_HEADER_TEXT}"
        "${REQUIRED_ENTITY_API_FRAGMENT}" FRAGMENT_OFFSET)
    if(FRAGMENT_OFFSET EQUAL -1)
        message(FATAL_ERROR
            "Missing typed Home Assistant entity API: ${REQUIRED_ENTITY_API_FRAGMENT}")
    endif()
endforeach()

string(FIND "${HOME_ASSISTANT_HEADER_TEXT}"
    "std::function<void(std::vector<Entity>, std::string" LEGACY_ENTITY_API_OFFSET)
if(NOT LEGACY_ENTITY_API_OFFSET EQUAL -1)
    message(FATAL_ERROR
        "Entity fetch must not expose backend English strings through its public API")
endif()

foreach(REQUIRED_EXECUTOR_FRAGMENT IN ITEMS
        "class BoundedTaskExecutor"
        "class OnceUiCompletion"
        "submit_with_discard"
        "m_tasks.size() >= m_max_pending"
        "m_cancel_requested.store(true)"
        "discarded.swap(m_tasks)"
        "worker.join()"
        "kMaxEntityQueryDomains = 4"
        "kMaxEntityFetchResults = 512"
        "kMaxConfiguredEntityValues = 32"
        "kMaxConfiguredEntitySegments = 256"
        "kMaxConfiguredEntityScanBytes = 64 * 1024"
        "bounded_semicolon_config_values"
        "inspected_segments < segment_limit"
        "scan_end = std::min(raw.size(), scan_byte_limit)"
        "raw.begin() + scan_end"
        "parse_filtered_entity_states"
        "struct LightAlertTransaction"
        "run_light_alert_transaction"
        "kMaxLightEntityIds = 32"
        "{\"entity_id\", light_entity_ids}"
        "bambustudio_light_restore_"
        "class LightAlertExecutor"
        "queued.target_url == transaction.target_url"
        "queued.generation == transaction.generation"
        "*superseded = std::move(transaction)"
        "m_pending.clear()"
        "kShutdownRecoveryTimeoutSeconds   = 2"
        "allow_during_shutdown")
    string(FIND "${EXECUTOR_TEXT}" "${REQUIRED_EXECUTOR_FRAGMENT}" FRAGMENT_OFFSET)
    if(FRAGMENT_OFFSET EQUAL -1)
        message(FATAL_ERROR
            "Missing transaction-executor safety behavior: ${REQUIRED_EXECUTOR_FRAGMENT}")
    endif()
endforeach()

string(FIND "${EXECUTOR_TEXT}" "/api/services/scene/create" SNAPSHOT_OFFSET)
string(FIND "${EXECUTOR_TEXT}" "/api/services/light/turn_on" FLASH_OFFSET)
string(FIND "${EXECUTOR_TEXT}" "/api/services/scene/turn_on" RESTORE_OFFSET)
string(FIND "${EXECUTOR_TEXT}" "/api/services/scene/delete" DELETE_OFFSET)
if(SNAPSHOT_OFFSET EQUAL -1 OR FLASH_OFFSET EQUAL -1 OR
   RESTORE_OFFSET EQUAL -1 OR DELETE_OFFSET EQUAL -1 OR
   NOT SNAPSHOT_OFFSET LESS FLASH_OFFSET OR
   NOT FLASH_OFFSET LESS RESTORE_OFFSET)
    message(FATAL_ERROR
        "Light alert phases must remain snapshot -> flash -> restore ordered and retain cleanup")
endif()

foreach(REQUIRED_LIGHT_CLEANUP_TEST IN ITEMS
        "Cancelled light alert deletes a snapshot that was never flashed"
        "Failed light restoration keeps its recovery scene")
    string(FIND "${TEST_TEXT}" "${REQUIRED_LIGHT_CLEANUP_TEST}" TEST_OFFSET)
    if(TEST_OFFSET EQUAL -1)
        message(FATAL_ERROR
            "Missing light-scene cleanup regression: ${REQUIRED_LIGHT_CLEANUP_TEST}")
    endif()
endforeach()

foreach(REQUIRED_UI_FRAGMENT IN ITEMS
        "kMaxConfiguredHomeAssistantEntities = 32"
        "AppendConfigResult::LimitReached"
        "std::string updated = cfg->get(key)"
        "if (values.size() > kMaxConfiguredHomeAssistantEntities)"
        "index < values.size() && index < kMaxConfiguredHomeAssistantEntities"
        "entries were kept; only the first 32 valid entries are active."
        "localized_entity_fetch_error"
        "localized_sharing_start_error"
        "HomeAssistant::SharingStartErrorCode"
        "Home Assistant request failed: %s"
        "You can configure up to 32 announcement speakers and 32 alert lights."
        "Printer handover is limited to the first 32 accessible printers")
    string(FIND "${SMART_HOME_TEXT}" "${REQUIRED_UI_FRAGMENT}" FRAGMENT_OFFSET)
    if(FRAGMENT_OFFSET EQUAL -1)
        message(FATAL_ERROR
            "Missing visible 32-item configuration cap: ${REQUIRED_UI_FRAGMENT}")
    endif()
endforeach()

foreach(FORBIDDEN_UI_FRAGMENT IN ITEMS
        "normalize_config_list"
        "joined_config_list"
        "values.resize(kMaxConfiguredHomeAssistantEntities)")
    string(FIND "${SMART_HOME_TEXT}" "${FORBIDDEN_UI_FRAGMENT}" FRAGMENT_OFFSET)
    if(NOT FRAGMENT_OFFSET EQUAL -1)
        message(FATAL_ERROR
            "Smart Home must not rewrite saved entity lists on open: ${FORBIDDEN_UI_FRAGMENT}")
    endif()
endforeach()

string(REGEX MATCHALL "HomeAssistant::shutdown\\(\\)" GUI_SHUTDOWN_SITES
    "${GUI_APP_TEXT}")
list(LENGTH GUI_SHUTDOWN_SITES GUI_SHUTDOWN_SITE_COUNT)
if(GUI_SHUTDOWN_SITE_COUNT LESS 2)
    message(FATAL_ERROR
        "Home Assistant must shut down in both GUI_App shutdown and OnExit fallback")
endif()

foreach(REQUIRED_TEST IN ITEMS
        "Bounded Home Assistant executor cancels active work and drops inert pending work"
        "Bounded Home Assistant executor contains task exceptions"
        "Home Assistant UI completion posts exactly once"
        "Discarded Home Assistant work posts one shutdown completion"
        "Entity state parsing validates domains and caps worker results"
        "Configured Home Assistant entities stop at thirty two unique values"
        "Configured Home Assistant entity parsing stops after its scan budget"
        "Light alert transaction is snapshot flash restore ordered"
        "Light alert transaction batches and caps thirty two entities"
        "Interrupted light alert restores its own generation immediately"
        "Failed light restoration keeps its recovery scene"
        "Light alert executor supersedes a pending target with one immutable newer generation"
        "Light alert executor contains request exceptions")
    string(FIND "${TEST_TEXT}" "${REQUIRED_TEST}" TEST_OFFSET)
    if(TEST_OFFSET EQUAL -1)
        message(FATAL_ERROR "Missing deterministic executor test: ${REQUIRED_TEST}")
    endif()
endforeach()

foreach(REQUIRED_SHUTDOWN_CONTRACT IN ITEMS
        "Entity queries that were accepted still attempt exactly one typed"
        "`ShuttingDown` completion through the UI queue"
        "late printer-import/convenience callbacks are suppressed")
    string(FIND "${HOME_ASSISTANT_HEADER_TEXT}"
        "${REQUIRED_SHUTDOWN_CONTRACT}" CONTRACT_OFFSET)
    if(CONTRACT_OFFSET EQUAL -1)
        message(FATAL_ERROR
            "Home Assistant shutdown callback contract is ambiguous: ${REQUIRED_SHUTDOWN_CONTRACT}")
    endif()
endforeach()

string(FIND "${HOME_ASSISTANT_TEXT}" "void add_printers(" ADD_PRINTERS_OFFSET)
if(ADD_PRINTERS_OFFSET EQUAL -1)
    message(FATAL_ERROR "Missing Home Assistant printer import entry point")
endif()
string(SUBSTRING "${HOME_ASSISTANT_TEXT}" ${ADD_PRINTERS_OFFSET} -1 ADD_PRINTERS_TEXT)
string(FIND "${ADD_PRINTERS_TEXT}" "wxTheApp->CallAfter" UNSAFE_PRINTER_POST_OFFSET)
if(NOT UNSAFE_PRINTER_POST_OFFSET EQUAL -1)
    message(FATAL_ERROR
        "Printer import must route every completion through the guarded UI poster")
endif()

string(FIND "${HOME_ASSISTANT_TEXT}" "boost::split" CONFIG_SPLIT_OFFSET)
if(NOT CONFIG_SPLIT_OFFSET EQUAL -1)
    message(FATAL_ERROR
        "Configured Home Assistant entities must be scanned only until the bounded active set is full")
endif()

# Service transactions, entity queries, light alerts, and printer imports are
# all owned and joined before wx teardown.
string(REGEX MATCHALL "\\.detach\\(\\)" DETACHED_SITES "${HOME_ASSISTANT_TEXT}")
list(LENGTH DETACHED_SITES DETACHED_SITE_COUNT)
if(NOT DETACHED_SITE_COUNT EQUAL 0)
    message(FATAL_ERROR
        "Home Assistant must not retain detached workers; found ${DETACHED_SITE_COUNT}")
endif()

string(FIND "${HOME_ASSISTANT_TEXT}" "wait_until" SOURCE_WAIT_UNTIL)
string(FIND "${EXECUTOR_TEXT}" "wait_until" EXECUTOR_WAIT_UNTIL)
if(NOT SOURCE_WAIT_UNTIL EQUAL -1 OR NOT EXECUTOR_WAIT_UNTIL EQUAL -1)
    message(FATAL_ERROR
        "Fragile phase deadlines must not return to Home Assistant executors")
endif()

message(STATUS
    "Home Assistant service fan-out contract passed: whole bounded transactions, ordered generation-safe light restore, active cancellation, UI caps, and pre-wx shutdown")
