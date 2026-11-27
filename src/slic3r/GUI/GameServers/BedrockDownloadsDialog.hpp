#pragma once
#include <string>
#include <functional>
class wxWindow;
namespace Slic3r { namespace GUI { namespace GameServers {
void show_bedrock_distributions(wxWindow *parent, std::function<void(const std::string &)> installed);
}}}
