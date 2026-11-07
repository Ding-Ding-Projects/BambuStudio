#pragma once

#include <nlohmann/json_fwd.hpp>

class wxWindow;
class wxSimplebook;

namespace Slic3r { namespace GUI { namespace PreferencesGeometryProbe {
// UI-thread only. Only these explicitly supplied containers are measured.
void bind(wxWindow* dialog, wxWindow* card, wxSimplebook* book,
          wxWindow* search, wxWindow* strip);
nlohmann::json snapshot();
}}}
