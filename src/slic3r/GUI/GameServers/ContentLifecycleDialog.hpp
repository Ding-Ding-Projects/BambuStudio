#pragma once
#include <string>
class wxWindow;
namespace Slic3r { namespace GUI { namespace GameServers {
void show_bedrock_content_inventory(wxWindow *parent,const std::string &root);
void show_java_content_inventory(wxWindow *parent,const std::string &root,const std::string &flavour);
}}}
