#pragma once
#include <filesystem>
class wxWindow;
namespace Slic3r::GUI {
// state_root must be the application's fixed private data directory, never a project path.
void show_ollama_suite(wxWindow *parent, const std::filesystem::path &state_root);
}
