# Source boundary guard paired with runtime UI validation. This does not prove pixels.
if(NOT BAMBU_SOURCE_DIR)
    message(FATAL_ERROR "BAMBU_SOURCE_DIR is required")
endif()
file(READ "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/Label.cpp" label_source)
function(native_setters_are_original source result)
    string(REGEX MATCHALL "wxStaticText::SetLabel\\([^;]*" setters "${source}")
    foreach(setter IN LISTS setters)
        if(NOT setter STREQUAL "wxStaticText::SetLabel(label)" AND
           NOT setter STREQUAL "wxStaticText::SetLabel({})" AND
           NOT setter STREQUAL "wxStaticText::SetLabel(wrapper.GetText())")
            set(${result} FALSE PARENT_SCOPE)
            return()
        endif()
    endforeach()
    if(NOT source MATCHES "wrapper.Wrap\\(this, m_text, width\\)" OR
       NOT source MATCHES "void Label::OnPersonalVocabularyPaint" OR
       NOT source MATCHES "dc.DrawLabel\\(wxControl::RemoveMnemonics\\(PersonalDisplayText\\(\\)\\)")
        set(${result} FALSE PARENT_SCOPE)
        return()
    endif()
    set(${result} TRUE PARENT_SCOPE)
endfunction()
native_setters_are_original("${label_source}" good)
if(NOT good)
    message(FATAL_ERROR "Personal display text can reach native Label getters")
endif()
# Reintroduce the reviewed regression and prove this guard rejects it.
string(REPLACE "wxStaticText::SetLabel(label);"
    "wxStaticText::SetLabel(Slic3r::GUI::PersonalVocabulary::display(label));"
    mutated_source "${label_source}")
native_setters_are_original("${mutated_source}" bad)
if(bad)
    message(FATAL_ERROR "Native display boundary mutation was not rejected")
endif()
message(STATUS "Native display getter source boundary and negative mutation passed")
