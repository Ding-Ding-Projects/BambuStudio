# Source contract for the renamable app display name.
#
# The display name is a label. Identity-bound code (data directory, config and
# log file names, updater/Squirrel ids, HTTP user agents, diagnostic report
# headers, file associations) must keep reading the compiled-in product name and
# must never touch the "app_display_name" key or the GUI accessor. Conversely the
# presentational surfaces named in the feature doc must use the accessor rather
# than the constant. Both halves are hand-listed here: a rule that only checks
# files that reference the key would pass on a file that never mentions it.
if(NOT DEFINED BAMBU_SOURCE_DIR)
    message(FATAL_ERROR "BAMBU_SOURCE_DIR is required")
endif()

cmake_policy(SET CMP0007 NEW)
set(_src "${BAMBU_SOURCE_DIR}/src")

# --- Identity-bound files: no display-name reference of any kind -------------
set(identity_files
    "${_src}/libslic3r/utils.cpp"                 # data_dir(), header_slic3r_generated()
    "${_src}/libslic3r/AppConfig.cpp"             # BambuStudio.conf path
    "${_src}/libslic3r/LogSink.cpp"               # log header app_name
    "${_src}/libslic3r/Config.cpp"                # G-code header
    "${_src}/libslic3r/Format/bbs_3mf.cpp"        # 3MF Application metadata
    "${_src}/libslic3r/Format/3mf.cpp"
    "${_src}/libslic3r/GCode/GCodeProcessor.cpp"  # producer detection
    "${_src}/BambuStudio.cpp"                     # CLI, --datadir default, fatal-error caption
    "${_src}/slic3r/Utils/Http.cpp"               # User-Agent
    "${_src}/slic3r/Utils/PresetUpdater.cpp"      # update feed logging
    "${_src}/slic3r/Utils/HelioDragon.cpp"        # client-name headers
    "${_src}/slic3r/GUI/SendSystemInfoDialog.cpp" # diagnostic report app_name
    "${_src}/slic3r/GUI/SysInfoDialog.cpp"        # system information copied into bug reports
    "${_src}/slic3r/GUI/BackgroundSlicingProcess.cpp" # temp upload file names
    "${_src}/slic3r/GUI/GCodeRenderer/AdvancedRenderer.cpp"
    "${_src}/slic3r/GUI/GCodeRenderer/LegacyRenderer.cpp"
)
foreach(f IN LISTS identity_files)
    if(NOT EXISTS "${f}")
        message(FATAL_ERROR "identity contract: listed file is missing: ${f}")
    endif()
    file(READ "${f}" src)
    if(src MATCHES "app_display_name")
        message(FATAL_ERROR "identity contract: ${f} references the display name; identity-bound code must keep SLIC3R_APP_NAME/SLIC3R_APP_FULL_NAME")
    endif()
    if(src MATCHES "AppDisplayName")
        message(FATAL_ERROR "identity contract: ${f} includes the AppDisplayName module")
    endif()
endforeach()

# --- Registry / file association / updater identity -------------------------
# GUI_App.cpp hosts both the accessor and identity code, so check the specific
# identity lines rather than the whole file.
file(READ "${_src}/slic3r/GUI/GUI_App.cpp" gui_app)
foreach(required
        "SetAppName\\(SLIC3R_APP_KEY\\)"                           # wx app name -> data dir, config
        "X-BBL-Client-Name\", SLIC3R_APP_NAME"                     # cloud client identity
        "header_json\\[\"name\"\\] = std::string\\(SLIC3R_APP_NAME\\)" # diagnostic header
        "std::wstring prog_desc = L\"BambuStudio\""                 # file-association ProgID description
        )
    if(NOT gui_app MATCHES "${required}")
        message(FATAL_ERROR "identity contract: GUI_App.cpp lost identity line: ${required}")
    endif()
endforeach()
# The accessor itself must fall back to the compiled-in constant, and the setter
# must persist under the documented key.
foreach(required
        "wxString GUI_App::app_display_name\\(\\) const"
        "const std::string shipped = SLIC3R_APP_FULL_NAME;"
        "AppDisplayName::resolve\\(app_config->get\\(AppDisplayName::CONFIG_KEY\\), shipped\\)"
        "bool GUI_App::set_app_display_name\\(const std::string &candidate\\)"
        "wxDEFINE_EVENT\\(EVT_APP_DISPLAY_NAME_CHANGED, wxCommandEvent\\)"
        "mainframe->on_app_display_name_changed\\(\\)"
        )
    if(NOT gui_app MATCHES "${required}")
        message(FATAL_ERROR "accessor contract: GUI_App.cpp is missing: ${required}")
    endif()
endforeach()

file(READ "${_src}/slic3r/GUI/AppDisplayName.hpp" hdr)
if(NOT hdr MATCHES "constexpr const char \\*CONFIG_KEY = \"app_display_name\";")
    message(FATAL_ERROR "AppDisplayName.hpp must define CONFIG_KEY as \"app_display_name\"")
endif()
if(hdr MATCHES "#include <wx/" OR hdr MATCHES "#include \"libslic3r")
    message(FATAL_ERROR "AppDisplayName.hpp must stay free of wx and libslic3r includes so the rules are unit-testable")
endif()

# --- Presentational surfaces: must read the accessor, not the constant --------
# (file . required-pattern . forbidden-pattern)
set(presentational
    "BBLTopbar.cpp|AddTool\\(ID_LOGO, wxGetApp\\(\\)\\.app_display_name\\(\\)|_L\\(\"Bambu Studio\"\\)"
    "BBLTopbar.cpp|void BBLTopbar::SetBrandLabel\\(const wxString& label\\)|"
    "MainFrame.cpp|SetTitle\\(title \\+ \" - \" \\+ wxGetApp\\(\\)\\.app_display_name\\(\\)\\)|SetTitle\\(title \\+ \" - BambuStudio\"\\)"
    "MainFrame.cpp|void MainFrame::on_app_display_name_changed\\(\\)|"
    "MainFrame.cpp|_L\\(\"&About %s\"\\), wxGetApp\\(\\)\\.app_display_name\\(\\)|_L\\(\"&About %s\"\\), SLIC3R_APP_FULL_NAME"
    "AboutDialog.cpp|into_u8\\(wxGetApp\\(\\)\\.app_display_name\\(\\)\\)|\\? SLIC3R_APP_FULL_NAME :"
    "MsgDialog.cpp|L\\(\"%s error\"\\)\\), wxGetApp\\(\\)\\.app_display_name\\(\\)|SLIC3R_APP_FULL_NAME"
    "GUI.cpp|wxGetApp\\(\\)\\.app_display_name\\(\\) \\+ \" - \" \\+ \\(title\\.empty\\(\\)|wxString\\(SLIC3R_APP_FULL_NAME \" - \"\\)"
    "Preferences.cpp|AppDisplayName::CONFIG_KEY|"
    "Preferences.cpp|_L\\(\"Reset to shipped name\"\\)|"
    "Preferences.cpp|wxGetApp\\(\\)\\.set_app_display_name\\(std::string\\(\\)\\)|"
    "Preferences.cpp|_L\\(\"App name\"\\)|"
)
foreach(entry IN LISTS presentational)
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 fname)
    list(GET parts 1 required)
    list(LENGTH parts nparts)
    set(forbidden "")
    if(nparts GREATER 2)
        list(GET parts 2 forbidden)
    endif()
    file(READ "${_src}/slic3r/GUI/${fname}" body)
    if(NOT body MATCHES "${required}")
        message(FATAL_ERROR "display-name contract: ${fname} is missing: ${required}")
    endif()
    if(forbidden AND body MATCHES "${forbidden}")
        message(FATAL_ERROR "display-name contract: ${fname} still hard-codes the product name: ${forbidden}")
    endif()
endforeach()

# The Preferences field must localise its captions through _L() but must not
# translate the shipped name itself.
file(READ "${_src}/slic3r/GUI/Preferences.cpp" prefs)
if(prefs MATCHES "_L\\(SLIC3R_APP_FULL_NAME\\)" OR prefs MATCHES "_L\\(\"Bambu Studio\"\\)")
    message(FATAL_ERROR "display-name contract: the shipped product name must not be passed through the translation catalog")
endif()

message(STATUS "app_display_name identity contract: OK")
