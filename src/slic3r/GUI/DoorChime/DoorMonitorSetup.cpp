#include "DoorMonitorSetup.hpp"

// A header in the precompiled set defines pid_t as a macro on Windows, which
// breaks Boost.Process's own typedef of that name.
#ifdef pid_t
#undef pid_t
#endif
#include <boost/process.hpp>
#ifdef _WIN32
#include <boost/process/windows.hpp>
#endif

#include "libslic3r/Utils.hpp"
#include "slic3r/Utils/Http.hpp"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/fstream.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <random>
#include <regex>
#include <sstream>
#include <thread>
#include <vector>

namespace Slic3r { namespace GUI { namespace DoorMonitor {

namespace {

namespace bp = boost::process;

// The same compose.yaml as the Printer Door Monitor repository. The printer
// details never go in here; they are handed to the running monitor instead.
constexpr const char *COMPOSE_YAML = R"(name: printer-door-monitor
services:
  door-monitor:
    image: ghcr.io/ding-ding-projects/printer-door-monitor:${DOOR_MONITOR_TAG:-latest}
    container_name: printer-door-monitor
    restart: unless-stopped
    ports:
      - "${DOOR_MONITOR_PORT:-8790}:8790"
    volumes:
      - door-monitor-data:/data
volumes:
  door-monitor-data:
)";

struct Output
{
    int         code = -1;
    std::string text;
};

// Runs one docker command against this computer or the SSH host and returns
// its exit code and combined output. `input`, when given, goes to its stdin.
Output docker(const std::vector<std::string> &args, const std::string &ssh, const std::string *input = nullptr)
{
    Output result;
    const boost::filesystem::path exe = bp::search_path("docker");
    if (exe.empty()) {
        result.text = "docker-missing";
        return result;
    }
    bp::environment env = boost::this_process::environment();
    if (ssh.empty())
        env.erase("DOCKER_HOST");
    else
        env["DOCKER_HOST"] = "ssh://" + ssh;
    env["DOOR_MONITOR_PORT"] = std::to_string(PORT);
    env["DOOR_MONITOR_TAG"]  = "latest";
    try {
        bp::ipstream out;
        bp::opstream in;
#ifdef _WIN32
        bp::child child(exe, bp::args(args), env, (bp::std_out & bp::std_err) > out, bp::std_in < in, bp::windows::create_no_window);
#else
        bp::child child(exe, bp::args(args), env, (bp::std_out & bp::std_err) > out, bp::std_in < in);
#endif
        if (input != nullptr) in << *input;
        in.flush();
        in.pipe().close();
        std::string line;
        while (std::getline(out, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            result.text += line + "\n";
        }
        child.wait();
        result.code = child.exit_code();
    } catch (const std::exception &err) {
        result.text += err.what();
    }
    return result;
}

std::string last_lines(const std::string &text, int count = 3)
{
    std::vector<std::string> lines;
    std::istringstream       stream(text);
    for (std::string line; std::getline(stream, line);)
        if (!line.empty()) lines.push_back(line);
    std::string tail;
    for (size_t i = lines.size() > size_t(count) ? lines.size() - count : 0; i < lines.size(); ++i) tail += lines[i] + "\n";
    return tail;
}

// The address of this computer on the network that reaches `host`, found by
// asking which interface a packet to it would leave from (nothing is sent).
std::string address_toward(const std::string &host)
{
    try {
        boost::asio::io_context        io;
        boost::asio::ip::udp::resolver resolver(io);
        boost::asio::ip::udp::socket   socket(io);
        socket.connect(*resolver.resolve(boost::asio::ip::udp::v4(), host, "8883").begin());
        return socket.local_endpoint().address().to_string();
    } catch (...) {
        return {};
    }
}

bool answers(const std::string &url, std::string &version)
{
    bool ok = false;
    Http::get(url + "healthz")
        .timeout_connect(5)
        .timeout_max(8)
        .on_complete([&](std::string body, unsigned status) {
            if (status != 200) return;
            const auto reply = nlohmann::json::parse(body, nullptr, false);
            if (reply.is_object() && reply.value("ok", false)) {
                ok      = true;
                version = reply.value("version", std::string());
            }
        })
        .on_error([](std::string, std::string, unsigned) {})
        .perform_sync();
    return ok;
}

// Sends a provisioning request to the monitor through docker exec and fills
// in the sign-in link from its answer.
bool provision(const std::string &ssh, const nlohmann::json &body, Result &result)
{
    const std::string input = body.dump();
    const Output      out   = docker({"exec", "-i", CONTAINER, "node", "src/provision-cli.mjs"}, ssh, &input);
    const std::string last  = last_lines(out.text, 1);
    const auto        reply = nlohmann::json::parse(last, nullptr, false);
    if (out.code != 0 || !reply.is_object() || !reply.value("ok", false)) {
        result.error  = Error::Configure;
        result.detail = reply.is_object() && reply.contains("error") ? reply["error"].dump() : last_lines(out.text);
        return false;
    }
    result.password_set = reply.value("passwordSet", false);
    const std::string login = reply.contains("login") && reply["login"].is_string() ? reply["login"].get<std::string>() : std::string();
    if (!login.empty() && login.front() == '/') result.login_url = result.url + login.substr(1);
    if (reply.contains("version") && reply["version"].is_string()) result.version = reply["version"].get<std::string>();
    return true;
}

Result failed(Error error, const std::string &detail = {})
{
    Result result;
    result.error  = error;
    result.detail = detail;
    BOOST_LOG_TRIVIAL(warning) << "Door monitor setup stopped (" << int(error) << "): " << detail;
    return result;
}

} // namespace

bool valid_ssh_target(const std::string &target)
{
    static const std::regex pattern(R"(^[A-Za-z0-9._-]+@[A-Za-z0-9.:-]+$)");
    return std::regex_match(target, pattern);
}

std::string generate_password()
{
    static const char alphabet[] = "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::random_device                      device;
    std::uniform_int_distribution<size_t>   pick(0, sizeof(alphabet) - 2);
    std::string                             password;
    for (int i = 0; i < 20; ++i) password += alphabet[pick(device)];
    return password;
}

Result setup(const Request &request, const std::function<void(Phase)> &on_phase, const std::atomic<bool> &cancel)
{
    const std::string &ssh = request.ssh;
    if (!ssh.empty() && !valid_ssh_target(ssh)) return failed(Error::BadTarget, ssh);

    on_phase(Phase::Check);
    const Output version = docker({"--version"}, ssh);
    if (version.code != 0) return failed(Error::DockerMissing, last_lines(version.text));
    if (docker({"compose", "version", "--short"}, ssh).code != 0) return failed(Error::ComposeMissing);
    const Output engine = docker({"info", "--format", "{{.ServerVersion}}"}, ssh);
    if (engine.code != 0) return failed(ssh.empty() ? Error::EngineStopped : Error::SshEngine, last_lines(engine.text));

    boost::filesystem::path dir = boost::filesystem::path(data_dir()) / "door_monitor";
    boost::system::error_code ec;
    boost::filesystem::create_directories(dir, ec);
    const boost::filesystem::path compose = dir / "compose.yaml";
    {
        boost::nowide::ofstream file(compose.string(), std::ios::binary | std::ios::trunc);
        file << COMPOSE_YAML;
    }

    if (cancel) return failed(Error::Pull, "cancelled");
    on_phase(Phase::Pull);
    const Output pull = docker({"compose", "-f", compose.string(), "pull"}, ssh);
    if (pull.code != 0) {
        // Offline, or the registry is down: a copy already on the target still works.
        if (docker({"image", "inspect", "--format", "{{.Id}}", std::string(IMAGE) + ":latest"}, ssh).code != 0)
            return failed(Error::Pull, last_lines(pull.text));
    }

    if (cancel) return failed(Error::Start, "cancelled");
    on_phase(Phase::Start);
    const Output up = docker({"compose", "-f", compose.string(), "up", "-d"}, ssh);
    if (up.code != 0) return failed(Error::Start, last_lines(up.text));

    on_phase(Phase::Health);
    bool healthy = false;
    for (int i = 0; i < 45 && !cancel; ++i) {
        const Output health = docker({"inspect", "--format", "{{.State.Health.Status}}", CONTAINER}, ssh);
        if (health.text.find("healthy") == 0) {
            healthy = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    if (!healthy) return failed(Error::Health, cancel ? "cancelled" : "");

    Result result;
    const std::string host = ssh.empty() ? address_toward(request.printer_host) : ssh.substr(ssh.find('@') + 1);
    result.url = "http://" + (host.empty() ? std::string("127.0.0.1") : host) + ":" + std::to_string(PORT) + "/";

    on_phase(Phase::Configure);
    nlohmann::json body = {
        {"printer", {{"host", request.printer_host}, {"serial", request.serial}, {"accessCode", request.access_code}}},
        {"chime", {{"enabled", true}, {"open", request.open_chime}, {"close", request.close_chime}}},
        {"setupDone", true},
    };
    if (!request.password.empty()) body["password"] = request.password;
    if (!provision(ssh, body, result)) return failed(result.error, result.detail);

    on_phase(Phase::Reach);
    std::string reached_version;
    if (!answers(result.url, reached_version)) return failed(Error::Reach, result.url);
    if (!reached_version.empty()) result.version = reached_version;
    return result;
}

Result sign_in_link(const std::string &ssh, const std::string &url)
{
    Result result;
    result.url = url;
    if (!ssh.empty() && !valid_ssh_target(ssh)) {
        result.error = Error::BadTarget;
        return result;
    }
    provision(ssh, nlohmann::json::object(), result);
    return result;
}

}}} // namespace Slic3r::GUI::DoorMonitor
