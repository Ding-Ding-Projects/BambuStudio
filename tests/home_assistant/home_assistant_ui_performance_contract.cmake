if(NOT DEFINED BAMBU_SOURCE_DIR)
    message(FATAL_ERROR "BAMBU_SOURCE_DIR is required")
endif()

set(smart_home_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/SmartHomeDialog.cpp")
set(smart_home_hpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/SmartHomeDialog.hpp")
file(READ "${smart_home_cpp}" smart_home_source)
file(READ "${smart_home_hpp}" smart_home_header)

# A slider drag can emit many events per second. Keep its network side effect
# behind one dialog-owned timer so rapid updates coalesce to the final value.
foreach(required_source_fragment IN ITEMS
        "constexpr int kVolumeRequestDebounceMs     = 200"
        "m_volume_debounce_timer.Bind(wxEVT_TIMER"
        "m_volume_debounce_timer.StartOnce(kVolumeRequestDebounceMs)"
        "if (m_entity_refresh_in_flight)"
        "m_entity_refresh_in_flight = true"
        "m_entity_refresh_pending = true"
        "dialog->m_entity_refresh_in_flight = false"
        "if (dialog->m_entity_refresh_pending)"
        "dialog->m_entity_refresh_pending = false"
        "wxLB_SINGLE | wxLB_HSCROLL"
        "kMaxVisibleHomeAssistantEntities    = 256"
        "kMaxConfiguredHomeAssistantSegments = 256"
        "kMaxConfiguredHomeAssistantScanBytes = 64 * 1024"
        "kMaxConfiguredHomeAssistantValueBytes = 256"
        "inspected_segments < kMaxConfiguredHomeAssistantSegments"
        "const std::size_t scan_end"
        "raw.begin() + scan_end"
        "bool *inspection_truncated = nullptr"
        "HomeAssistant::EntityQuery{{\"media_player\", \"light\"}}"
        "wxArrayString rows"
        "m_list->Freeze()"
        "m_list->Set(rows)"
        "m_list->Thaw()"
        "Showing %d of %d matching speakers and lights."
        "Refine the search to see the remaining matches.")
    string(FIND "${smart_home_source}" "${required_source_fragment}" fragment_offset)
    if(fragment_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing Home Assistant UI boundedness behavior: ${required_source_fragment}")
    endif()
endforeach()

foreach(required_header_fragment IN ITEMS
        "wxTimer      m_volume_debounce_timer"
        "std::string  m_pending_volume_entity"
        "Label       *m_results_status"
        "bool         m_entity_refresh_in_flight { false }"
        "bool         m_entity_refresh_pending { false }"
        "bool         m_entity_fetch_truncated { false }")
    string(FIND "${smart_home_header}" "${required_header_fragment}" fragment_offset)
    if(fragment_offset EQUAL -1)
        message(FATAL_ERROR
            "Missing Home Assistant UI boundedness state: ${required_header_fragment}")
    endif()
endforeach()

string(FIND "${smart_home_source}" "m_list->Append(" append_offset)
if(NOT append_offset EQUAL -1)
    message(FATAL_ERROR
        "SmartHomeDialog must replace bounded entity rows in one bulk Set() operation")
endif()

string(REGEX MATCHALL "HomeAssistant::media_volume\\(" volume_dispatches "${smart_home_source}")
list(LENGTH volume_dispatches volume_dispatch_count)
if(NOT volume_dispatch_count EQUAL 1)
    message(FATAL_ERROR
        "SmartHomeDialog must have exactly one debounced Home Assistant volume dispatch")
endif()

foreach(single_flight_pattern IN ITEMS
        "HomeAssistant::list_entities\\("
        "m_entity_refresh_pending = true"
        "dialog->m_entity_refresh_pending = false")
    string(REGEX MATCHALL "${single_flight_pattern}" single_flight_matches "${smart_home_source}")
    list(LENGTH single_flight_matches single_flight_match_count)
    if(NOT single_flight_match_count EQUAL 1)
        message(FATAL_ERROR
            "Entity refresh single-flight behavior must occur exactly once: ${single_flight_pattern}")
    endif()
endforeach()

string(FIND "${smart_home_source}" "m_volume_debounce_timer.Bind(wxEVT_TIMER" timer_bind_offset)
string(FIND "${smart_home_source}" "HomeAssistant::media_volume(" volume_dispatch_offset)
string(FIND "${smart_home_source}" "m_volume->Bind(wxEVT_SLIDER" slider_bind_offset)
if(volume_dispatch_offset LESS timer_bind_offset OR
   volume_dispatch_offset GREATER slider_bind_offset)
    message(FATAL_ERROR
        "The Home Assistant volume dispatch must live inside the debounce timer callback")
endif()

message(STATUS "Home Assistant UI performance contract passed")
