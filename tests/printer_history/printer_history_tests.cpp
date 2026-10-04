#include "slic3r/GUI/PrinterHistory.hpp"
#include "libslic3r/ProjectHistoryManager.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>
// Keep assertions active in release builds. This standalone executable uses
// engine stubs because snapshot creation is intentionally disabled in these
// lifecycle tests; it does not verify Git integration.
#undef assert
#define assert(condition) do { if (!(condition)) throw std::runtime_error("Assertion failed: " #condition); } while (false)
namespace Slic3r {
const std::string &data_dir() { static std::string p = "."; return p; }
class ProjectHistoryManager::Impl {};
ProjectHistoryManager::ProjectHistoryManager(std::filesystem::path) {}
ProjectHistoryManager::~ProjectHistoryManager() = default;
std::future<ProjectHistoryCommitResult> ProjectHistoryManager::commit_snapshot(std::filesystem::path, std::filesystem::path, ProjectHistoryCommitOptions) {
 return std::async(std::launch::deferred, [] { return ProjectHistoryCommitResult{}; });
}
}
using namespace Slic3r::GUI;
int main(int argc, char **argv) {
 assert(argc == 2);
 auto root = std::filesystem::path(argv[1]);
 std::filesystem::create_directories(root);
 auto path = root / "incidents.json";
 std::filesystem::remove(path);
 {
  PrinterHistory h(path, 3);
  h.observe("printer-one", {{"A", 2, "Incident A"}}, true, 1000);
  h.observe("printer-one", {{"A", 2, "Incident A"}}, true, 2000);
  assert(h.entries().size() == 1 && h.entries()[0].first_seen_ms == 1000 && h.entries()[0].last_seen_ms == 2000);
  h.observe("printer-one", {}, false, 3000);
  assert(h.entries()[0].state == "active");
  h.mark_unknown("printer-one", 4000);
  assert(h.entries()[0].state == "unknown" && h.entries()[0].resolved_ms == 0);
  h.observe("printer-one", {{"A", 2, "Incident A"}}, false, 5000);
  assert(h.entries().size() == 1 && h.entries()[0].state == "active");
  h.observe("printer-one", {{"E", 2, "Print error", "print_error"}}, true, 6000, "print_error");
  h.observe("printer-one", {}, true, 7000);
  auto rows=h.entries();
  assert(rows[0].state == "active" && rows[1].state == "resolved" && rows[1].resolved_ms == 7000);
  h.observe("printer-one", {{"A", 2, "Incident A"}}, true, 8000);
  assert(h.entries().size() == 3 && h.entries()[0].id != rows[1].id);
  h.observe("printer-two", {{"B", 3, "Incident B"}}, true, 9000);
  assert(h.entries().size() == 3 && h.entries("printer-two").size() == 1);
  assert(h.last_error().empty());
 }
 {
  PrinterHistory h(path, 3);
  auto rows=h.entries();
  assert(rows.size() == 3);
  for (auto &e:rows) assert(e.state == "unknown");
  h.observe("printer-one", {}, true, 10000, "print_error");
  rows=h.entries("printer-one");
  assert(rows.size() == 2 && rows[0].state == "unknown" && rows[1].state == "resolved");
 }
 auto corrupt = root / "corrupt.json";
 {std::ofstream f(corrupt); f << "{invalid";}
 {
  PrinterHistory h(corrupt);
  assert(!h.last_error().empty());
  h.observe("printer-one", {{"A",2,"A"}}, true, 1000);
 }
 {std::ifstream f(corrupt); std::string s; std::getline(f,s); assert(s=="{invalid");}
 auto strict_path = root / "strict.json";
 std::filesystem::remove(strict_path);
 {
  PrinterHistory h(strict_path);
  h.observe("printer-one", {{"A",2,"A"}}, true, 1000);
  h.observe("printer-one", {{"B",2,"B"}}, true, 2000);
  assert(h.entries().size() == 2 && h.entries()[1].state == "active");
  h.observe("printer-one", {}, false, 3000);
  assert(h.entries()[0].state == "active");
  h.observe("printer-one", {}, true, 4000);
  for (auto &e:h.entries()) assert(e.state == "resolved" && e.resolved_ms == 4000);
  h.observe("printer-one", {{"C",2,"C"}}, true, 5000);
  h.observe("printer-one", {}, true, 4500);
  assert(h.entries()[0].state == "active");
 }
 std::cout << "PASS: 10 lifecycle scenarios (dedup, partial report, unknown, recurrence, categories, retention, restart, corrupt preservation, complete empty HMS, stale report)\n";
}
