#pragma once
#include <filesystem>

namespace Slic3r::GUI::ConfigProfileArchive {
enum class ImportError { None, Open, Destination, UnsafePath, Write, Corrupt };
// Reserve a new destination and extract exclusively. Failure removes only paths
// created by this invocation; pre-existing files are never removed or replaced.
ImportError import_archive(const std::filesystem::path &archive, const std::filesystem::path &destination);
}
