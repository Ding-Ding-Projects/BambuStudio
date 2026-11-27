#include "TargetSettings.hpp"
#include "TargetSettingsScript.hpp"
#include "ContainerSetup.hpp"
#include "NativeServerTarget.hpp"
#include "ServerSystem.hpp"
#include <condition_variable>
#include <filesystem>

namespace Slic3r { namespace GUI { namespace GameServers {
namespace {
struct Result
{
    int                     code = -1;
    bool                    done = false;
    std::mutex              mutex;
    std::condition_variable signal;
    std::string             output;
};
std::pair<int, std::string> execute(const std::vector<std::string> &args, const std::string &stdin_line)
{
    ServerProcess p;
    auto          state = std::make_shared<Result>();
    std::string   error;
    if (args.empty() || !p.start(
                            args, std::filesystem::current_path().u8string(),
                            [state](const std::string &line) {
                                if (state->output.size() + line.size() < 256 * 1024) state->output += line + "\n";
                            },
                            [state](int code) {
                                std::lock_guard<std::mutex> lock(state->mutex);
                                state->code = code;
                                state->done = true;
                                state->signal.notify_all();
                            },
                            &error))
        return {-1, error};
    if (!stdin_line.empty() && !p.send(stdin_line)) {
        p.kill();
        return {-1, "Could not send the selected settings request."};
    }
    std::unique_lock<std::mutex> lock(state->mutex);
    if (!state->signal.wait_for(lock, std::chrono::seconds(60), [&] { return state->done; })) {
        p.kill();
        return {-1, "The target did not answer within 60 seconds."};
    }
    return {state->code, state->output};
}
TargetSettingsResult result(const std::pair<int, std::string> &op)
{
    if (op.first != 0) return {op.second.empty() ? "The target returned an unsuccessful exit code." : op.second, {}};
    auto end = op.second.find_last_not_of("\r\n");
    if (end == std::string::npos) return {"The target returned no settings result.", {}};
    auto start = op.second.rfind('\n', end);
    auto json  = nlohmann::json::parse(op.second.substr(start == std::string::npos ? 0 : start + 1), nullptr, false);
    return json.is_object() ? TargetSettingsResult{{}, std::move(json)} : TargetSettingsResult{"The settings result was invalid.", {}};
}
} // namespace

TargetSettingsResult target_settings(const WorldRecoveryTarget &t, const std::string &exe, nlohmann::json request)
{
    if (!request.is_object() || !request.contains("mode") || !request["mode"].is_string()) return {"Invalid settings action.", {}};
    const bool write = request["mode"] == "write";
    if (!t.container) {
        if (!ownership_id(t.owner) || shell_path(t.server_folder).empty()) return {"The selected SSH directory has no local ownership record.", {}};
        auto args = ssh_invocation(exe, t.profile, "exec python3 -c " + shell_literal(target_settings_script) + " " + shell_path(t.server_folder) + " " + t.owner + " ssh");
        return result(execute(args, request.dump()));
    }
    if (!container_id(t.owner) || !container_field(t.profile)) return {"Invalid container target.", {}};
    auto status = execute({exe, "--context", t.profile, "inspect", "--format", "{{.State.Running}}", t.owner}, {});
    if (status.first != 0 || (write && status.second != "false\n")) return {"Stop the selected container before changing its settings.", {}};
    auto args = [&](const std::string &user, bool writable) {
        std::vector<std::string> a = {exe,    "--context",   t.profile,    "run", "--rm",           "--interactive",    "--network",
                                      "none", "--read-only", "--cap-drop", "ALL", "--security-opt", "no-new-privileges"};
        if (!user.empty()) a.insert(a.end(), {"--user", user});
        a.insert(a.end(), {"--volumes-from", t.owner + (writable ? ":rw" : ":ro"), "python:3.13-alpine", "python3", "-c", target_settings_script, "/data", t.owner, "container"});
        return a;
    };
    const auto who = result(execute(args({}, false), nlohmann::json({{"keys", nlohmann::json::array()}, {"mode", "identity"}}).dump()));
    if (!who.error.empty() || !who.result.contains("user") || !who.result["user"].is_string()) return {"Could not determine the server file owner.", {}};
    auto       user  = who.result["user"].get<std::string>();
    const auto colon = user.find(':');
    if (user.size() > 24 || colon == std::string::npos || colon == 0 || colon + 1 == user.size() || user.find(':', colon + 1) != std::string::npos ||
        user.find_first_not_of("0123456789:") != std::string::npos)
        return {"The target reported an invalid file owner.", {}};
    return result(execute(args(user, write), request.dump()));
}
}}} // namespace Slic3r::GUI::GameServers
