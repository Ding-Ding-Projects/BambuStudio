#pragma once
#include <chrono>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace GameServers {
template<class Cli, class Now, class Pause>
std::string managed_container_restart(const std::string &owner, bool bedrock, bool warning, int minutes, Cli docker, Now now_time, Pause pause)
{
    auto inspect = [&] { return docker({"inspect", "--format", "{{.State.Status}}|{{.State.ExitCode}}|{{.State.StartedAt}}|{{.Config.Image}}", owner}); };
    auto prior   = inspect();
    if (prior.first || prior.second.rfind("running|", 0) != 0) return "Skipped: the owned container was not confirmed running.";
    auto image = prior.second.substr(prior.second.rfind('|') + 1);
    if (image.rfind(bedrock ? "itzg/minecraft-bedrock-server:" : "itzg/minecraft-server:", 0) != 0) return "Skipped: server image did not match the scheduled edition.";
    const std::string command = warning ? "say Scheduled server restart in " + std::to_string(minutes) + " minute(s)." : "stop";
    auto              sent    = docker({"exec", owner, bedrock ? "send-command" : "rcon-cli", command});
    if (warning) return sent.first ? "The server did not acknowledge the warning." : "Warning sent to the owned container console.";
    const auto deadline = now_time() + std::chrono::seconds(60);
    while (now_time() < deadline) {
        auto now = inspect();
        if (now.first) return "Connection lost; container state is unknown. No start issued.";
        if (now.second.rfind("running|", 0) == 0 && now.second != prior.second) return "Engine already restarted the owned container. Game readiness is unverified.";
        if (now.second.rfind("exited|", 0) == 0) {
            if (now.second.rfind("exited|0|", 0) != 0) return "Unsuccessful exit; no replacement start issued.";
            auto start = docker({"start", owner});
            if (start.first) return "Graceful stop confirmed; subsequent container start failed.";
            auto after = inspect();
            return !after.first && after.second.rfind("running|", 0) == 0 && after.second != prior.second ?
                       "Replacement container process is running. Game readiness is unverified." :
                       "Start issued; replacement process has not been verified.";
        }
        if (sent.first && now.second == prior.second) return "The console rejected the stop; the container was not force-stopped.";
        pause();
    }
    return "Graceful stop did not complete within 60 seconds. No forced stop or replacement start issued.";
}
}}} // namespace Slic3r::GUI::GameServers
