if(NOT DEFINED BAMBU_SOURCE_DIR)
    message(FATAL_ERROR "BAMBU_SOURCE_DIR is required")
endif()

set(SERVICE_HEADER
    "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/HomeAssistantSharingService.hpp")
set(SERVICE_SOURCE
    "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/HomeAssistantSharingService.cpp")

foreach(REQUIRED_FILE IN ITEMS "${SERVICE_HEADER}" "${SERVICE_SOURCE}")
    if(NOT EXISTS "${REQUIRED_FILE}")
        message(FATAL_ERROR "Missing Home Assistant sharing source: ${REQUIRED_FILE}")
    endif()
endforeach()

file(READ "${SERVICE_HEADER}" HEADER_TEXT)
file(READ "${SERVICE_SOURCE}" SOURCE_TEXT)

foreach(REQUIRED_HEADER_TOKEN IN ITEMS
        "enum class SharingStartErrorCode"
        "mdns_sender_is_eligible_ipv4"
        "native_error"
        "max_offer_response_bytes"
        "max_pending_mdns_responses"
        "mdns_response_interval_ms"
        "mdns_query_burst"
        "mdns_query_refill_ms"
        "offer_response_burst"
        "offer_response_refill_ms")
    string(FIND "${HEADER_TEXT}" "${REQUIRED_HEADER_TOKEN}" TOKEN_OFFSET)
    if(TOKEN_OFFSET EQUAL -1)
        message(FATAL_ERROR
            "Missing measurable sharing amplification limit: ${REQUIRED_HEADER_TOKEN}")
    endif()
endforeach()

string(FIND "${HEADER_TEXT}" "std::string   error" LEGACY_ERROR_OFFSET)
if(NOT LEGACY_ERROR_OFFSET EQUAL -1)
    message(FATAL_ERROR
        "Sharing start failures must expose structured codes, not backend English prose")
endif()

foreach(REQUIRED_SOURCE_TOKEN IN ITEMS
        "start_failure"
        "SharingStartErrorCode::HttpEndpointUnavailable"
        "SharingStartErrorCode::MdnsUnavailable"
        "consume_offer_response_budget"
        "cached_printer_payload"
        "http::status::too_many_requests"
        "Retry-After"
        "valid_mdns_sender"
        "interface_prefix_length"
        "config.on_link_prefix_length"
        "rollback_startup_failure"
        "m_lifecycle_condition"
        "worker_to_join = std::move(m_worker)"
        "consume_mdns_query_budget"
        "mdns_response_pending"
        "m_mdns_send_queue.size() >= kMaxPendingMdnsResponses"
        "m_active_mdns_response.reset"
        "m_mdns_send_timer.cancel"
        "m_mdns_send_queue.clear"
        "m_next_mdns_send = {}")
    string(FIND "${SOURCE_TEXT}" "${REQUIRED_SOURCE_TOKEN}" TOKEN_OFFSET)
    if(TOKEN_OFFSET EQUAL -1)
        message(FATAL_ERROR
            "Missing sharing amplification safeguard: ${REQUIRED_SOURCE_TOKEN}")
    endif()
endforeach()

foreach(REQUIRED_TEST_TOKEN IN ITEMS
        "Sharing start failures use stable structured error codes"
        "mDNS sender policy accepts only usable hosts on the advertised link"
        "Concurrent sharing stops serialize join and cleanup"
        "mDNS query admission is globally token bounded before refill")
    file(READ
        "${BAMBU_SOURCE_DIR}/tests/home_assistant/home_assistant_tests.cpp"
        TEST_TEXT)
    string(FIND "${TEST_TEXT}" "${REQUIRED_TEST_TOKEN}" TEST_OFFSET)
    if(TEST_OFFSET EQUAL -1)
        message(FATAL_ERROR
            "Missing sharing amplification/error test: ${REQUIRED_TEST_TOKEN}")
    endif()
endforeach()

string(FIND "${SOURCE_TEXT}" "lifecycle_lock.unlock" UNSERIALIZED_STOP_OFFSET)
if(NOT UNSERIALIZED_STOP_OFFSET EQUAL -1)
    message(FATAL_ERROR
        "Sharing lifecycle ownership must remain explicit while a moved worker is joined and cleaned")
endif()

# Query admission, duplicate/queue rejection, and the global token bucket must
# all execute before make_mdns_response allocates its packet.
string(FIND "${SOURCE_TEXT}" "void do_mdns_receive()" RECEIVE_OFFSET)
if(RECEIVE_OFFSET EQUAL -1)
    message(FATAL_ERROR "Missing mDNS receive handler")
endif()
string(SUBSTRING "${SOURCE_TEXT}" ${RECEIVE_OFFSET} -1 RECEIVE_TEXT)
string(FIND "${RECEIVE_TEXT}" "valid_mdns_sender" SENDER_OFFSET)
string(FIND "${RECEIVE_TEXT}" "mdns_response_pending" PENDING_OFFSET)
string(FIND "${RECEIVE_TEXT}" "consume_mdns_query_budget" BUDGET_OFFSET)
string(FIND "${RECEIVE_TEXT}" "make_mdns_response(" ALLOCATION_OFFSET)
if(SENDER_OFFSET EQUAL -1 OR PENDING_OFFSET EQUAL -1 OR
   BUDGET_OFFSET EQUAL -1 OR ALLOCATION_OFFSET EQUAL -1 OR
   NOT SENDER_OFFSET LESS ALLOCATION_OFFSET OR
   NOT PENDING_OFFSET LESS ALLOCATION_OFFSET OR
   NOT BUDGET_OFFSET LESS ALLOCATION_OFFSET)
    message(FATAL_ERROR
        "mDNS sender, duplicate/queue, and token admission must precede response allocation")
endif()

string(FIND "${SOURCE_TEXT}" "duplicate->packet" REPLACEMENT_OFFSET)
if(NOT REPLACEMENT_OFFSET EQUAL -1)
    message(FATAL_ERROR
        "Immutable mDNS responses must be deduplicated before allocation, not replaced afterward")
endif()

# Every asynchronous mDNS response must pass through the one-in-flight
# scheduler. A direct async_send_to at a query or announcement call site would
# recreate an unbounded queue under a multicast flood.
string(REGEX MATCHALL "async_send_to" ASYNC_SEND_SITES "${SOURCE_TEXT}")
list(LENGTH ASYNC_SEND_SITES ASYNC_SEND_SITE_COUNT)
if(NOT ASYNC_SEND_SITE_COUNT EQUAL 1)
    message(FATAL_ERROR
        "Expected exactly one scheduled async_send_to site, found ${ASYNC_SEND_SITE_COUNT}")
endif()

message(STATUS
    "Home Assistant sharing amplification contract passed: pre-allocation mDNS admission, bounded paced sends, cached offers, and HTTP retry budget are present")
