#pragma once

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

namespace Slic3r::GUI::ModelCreator {

enum class ProcessRole { ClaudeCli, CodexCli, Renderer };

// Launches an exact executable and argv, with no shell. On Windows the process
// is suspended until assigned to a kill-on-close Job Object. Completion means
// every process in that job has exited, not merely the direct child.
bool run_isolated_process(const std::filesystem::path &program,
                          const std::vector<std::string> &args,
                          const std::filesystem::path &input,
                          const std::filesystem::path &output,
                          const std::filesystem::path &directory,
                          int timeout_seconds, std::atomic_bool &cancel,
                          ProcessRole role, std::string &error);

} // namespace Slic3r::GUI::ModelCreator
