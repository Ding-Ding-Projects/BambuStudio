#pragma once
#include <string>
class wxWindow;
namespace Slic3r { namespace GUI { namespace GameServers {
void show_content_catalog(wxWindow *parent, const std::string &folder, const std::string &flavour, const std::string &version);
}}}
