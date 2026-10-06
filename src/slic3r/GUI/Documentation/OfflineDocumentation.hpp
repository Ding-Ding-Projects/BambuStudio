#pragma once
#include <wx/string.h>
class wxWindow;
namespace Slic3r { namespace GUI { namespace Documentation {
// Routes are relative to docs/features, optionally followed by #heading.
// Returns false for a route not present in the immutable bundle.
bool ShowOfflineDocumentation(wxWindow* parent, const wxString& route = wxString());
}}}
