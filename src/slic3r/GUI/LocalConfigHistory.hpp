#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>
namespace Slic3r { class ProjectHistoryManager; namespace GUI { namespace LocalConfigHistory {
struct Source { std::string category, name; std::filesystem::path identity; };
// Records only recognized, non-sensitive printing options. Geometry remains
// exclusively in the existing per-project history store.
bool record(const std::string &category, const std::string &identity,
            const std::string &label, const std::map<std::string, std::string> &values);
std::vector<Source> sources();
ProjectHistoryManager *manager();
std::map<std::string, std::string> read_snapshot(const std::filesystem::path &path);
}}}
