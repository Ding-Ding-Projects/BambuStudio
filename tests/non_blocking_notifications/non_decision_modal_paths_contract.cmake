if(NOT DEFINED BAMBU_SOURCE_DIR)
    message(FATAL_ERROR "BAMBU_SOURCE_DIR is required")
endif()

set(create_presets_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/CreatePresetsDialog.cpp")
set(plater_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Plater.cpp")

file(READ "${create_presets_cpp}" create_presets_source)
file(READ "${plater_cpp}" plater_source)

foreach(required_info_message
        "show_info\\(this, _L\\(\"Vendor is not selected, please reselect vendor\\.\"\\), _L\\(\"Info\"\\)\\)"
        "show_info\\(this, _L\\(\"You need to select at least one filament preset\\.\"\\), _L\\(\"Info\"\\)\\)"
        "show_info\\(this, _L\\(\"Please select at least one printer or filament\\.\"\\), _L\\(\"Info\"\\)\\)"
        "show_info\\(this, _L\\(\"Export successful\"\\), _L\\(\"Info\"\\)\\)")
    if(NOT create_presets_source MATCHES "${required_info_message}")
        message(FATAL_ERROR "CreatePresetsDialog acknowledgement is not routed through show_info: ${required_info_message}")
    endif()
endforeach()

foreach(required_error_message
        "show_error\\(this, _L\\(\"initialize fail\"\\)\\)"
        "show_error\\(wxGetApp\\(\\)\\.mainframe, _L\\(\"Failed to create temporary folder, please try Export Configs again\\.\"\\)\\)"
        "show_error\\(this, msg \\+ presets\\)")
    if(NOT create_presets_source MATCHES "${required_error_message}")
        message(FATAL_ERROR "CreatePresetsDialog error acknowledgement is not routed through show_error: ${required_error_message}")
    endif()
endforeach()

if(create_presets_source MATCHES "MessageDialog[^\n]*Vendor is not selected, please reselect vendor")
    message(FATAL_ERROR "Filament validation regressed to a modal acknowledgement")
endif()

if(NOT plater_source MATCHES "show_info\\(q, _L\\(\"Objects with zero volume removed\"\\), _L\\(\"The volume of the object is zero\"\\)\\)")
    message(FATAL_ERROR "Zero-volume import acknowledgement is not routed through show_info")
endif()
if(plater_source MATCHES "MessageDialog\\(q, _L\\(\"Objects with zero volume removed\"")
    message(FATAL_ERROR "Zero-volume import regressed to a modal acknowledgement")
endif()

foreach(required_decision
        "if \\(wxID_YES != dlg\\.ShowModal\\(\\)\\)"
        "if \\(dlg\\.ShowModal\\(\\) == wxID_YES\\)"
        "MessageDialog\\(this, msg, _L\\(\"Delete preset\"\\), wxYES_NO")
    if(NOT create_presets_source MATCHES "${required_decision}")
        message(FATAL_ERROR "A decision-bearing preset confirmation is no longer modal: ${required_decision}")
    endif()
endforeach()

# The import unit conversion is no longer a decision. Upstream Bambu Studio
# ("ENH: convert units when in doubt after loading models", cherry-picked as
# ec547f893) converts a model that looks saved in metres or in inches to
# millimetres without asking, and logs it. With no choice left to make there is
# nothing to keep modal: both conversions run unasked and logged, and the old
# "Object too small" question must not come back as a modal or any other prompt.
if(NOT plater_source MATCHES "looks_like_saved_in_meters\\(\\)\\) \\{[ \t\r\n]*BOOST_LOG_TRIVIAL\\(warning\\)[^\n]*[ \t\r\n]*model\\.convert_from_meters\\(true\\);")
    message(FATAL_ERROR "A model that looks saved in metres is no longer converted unasked and logged")
endif()
if(NOT plater_source MATCHES "looks_like_imperial_units\\(\\)\\) \\{[ \t\r\n]*BOOST_LOG_TRIVIAL\\(warning\\)[^\n]*[ \t\r\n]*convert_from_imperial_units\\(model, true\\);")
    message(FATAL_ERROR "A model that looks saved in inches is no longer converted unasked and logged")
endif()
if(plater_source MATCHES "_L\\(\"Object too small\"\\)")
    message(FATAL_ERROR "The import unit conversion asks again: the Object too small prompt is back")
endif()

message(STATUS "Non-decision modal paths contract passed")
