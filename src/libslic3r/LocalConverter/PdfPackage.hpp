#pragma once
#include <filesystem>
#include <string>
namespace Slic3r::LocalConverter {
bool verify_pdf_package(const std::filesystem::path &installed_directory,std::string &reason);
// Call only inside the converter worker after AppContainer verification.
bool load_pdf_engine(std::string &code);
}
