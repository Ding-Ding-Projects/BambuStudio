#include "ModelCreatorBackend.hpp"

#include <boost/process.hpp>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <random>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#endif

namespace Slic3r::GUI::ModelCreator {
namespace {
namespace bp = boost::process;
using json = nlohmann::json;
constexpr size_t max_response = 65536;

std::string target(Provider provider)
{
    return provider == Provider::AnthropicApi ? "BambuStudio.ModelCreator.Anthropic" :
           provider == Provider::OpenAiApi ? "BambuStudio.ModelCreator.OpenAI" : "";
}

std::string load_api_key(Provider provider)
{
#ifdef _WIN32
    const std::string id = target(provider);
    const std::wstring name(id.begin(), id.end());
    PCREDENTIALW credential = nullptr;
    if (name.empty() || !CredReadW(name.c_str(), CRED_TYPE_GENERIC, 0, &credential)) return {};
    std::string key(reinterpret_cast<char *>(credential->CredentialBlob), credential->CredentialBlobSize);
    CredFree(credential);
    return key;
#else
    return {};
#endif
}

size_t append_response(char *bytes, size_t size, size_t count, void *context)
{
    auto &out = *static_cast<std::string *>(context);
    const size_t length = size * count;
    if (length > max_response || out.size() > max_response - length) return 0;
    out.append(bytes, length);
    return length;
}

std::string api_request(const Settings &settings, const std::string &prompt, std::atomic_bool &cancel,
                        std::string &error)
{
    const std::string key = load_api_key(settings.provider);
    if (key.empty()) { error = "No key is saved for this provider"; return {}; }
    const bool anthropic = settings.provider == Provider::AnthropicApi;
    json body = anthropic ?
        json{{"model", settings.model}, {"max_tokens", 4096},
             {"messages", json::array({{{"role", "user"}, {"content", prompt}}})}} :
        json{{"model", settings.model}, {"input", prompt}, {"max_output_tokens", 4096}};
    std::string response;
    CURL *curl = curl_easy_init();
    if (!curl) { error = "HTTP client unavailable"; return {}; }
    curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, anthropic ? "anthropic-version: 2023-06-01" : "Accept: application/json");
    const std::string auth = anthropic ? "x-api-key: " + key : "Authorization: Bearer " + key;
    headers = curl_slist_append(headers, auth.c_str());
    const std::string payload = body.dump();
    curl_easy_setopt(curl, CURLOPT_URL, anthropic ? "https://api.anthropic.com/v1/messages" :
                                                  "https://api.openai.com/v1/responses");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(settings.timeout_seconds));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,
                     +[](void *ctx, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
                         return static_cast<std::atomic_bool *>(ctx)->load() ? 1 : 0;
                     });
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancel);
    const CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (cancel) { error = "Canceled"; return {}; }
    if (code != CURLE_OK || status < 200 || status >= 300) {
        error = "Provider request failed (HTTP " + std::to_string(status) + ")"; return {};
    }
    auto parsed = json::parse(response, nullptr, false);
    if (parsed.is_discarded()) { error = "Provider response is not JSON"; return {}; }
    try {
        if (anthropic) {
            for (const auto &item : parsed.at("content"))
                if (item.value("type", "") == "text") return item.at("text").get<std::string>();
        } else {
            for (const auto &item : parsed.at("output"))
                if (item.value("type", "") == "message")
                    for (const auto &part : item.at("content"))
                        if (part.value("type", "") == "output_text") return part.at("text").get<std::string>();
        }
    } catch (const json::exception &) {}
    error = "Provider did not return text";
    return {};
}

bool execute(const std::filesystem::path &program, const std::vector<std::string> &args,
             const std::filesystem::path &in, const std::filesystem::path &out,
             const std::filesystem::path &cwd, int seconds, std::atomic_bool &cancel,
             std::string &error)
{
    if (!std::filesystem::is_regular_file(program)) { error = "Executable not found"; return false; }
    try {
        bp::child child(program.string(), bp::args(args), bp::std_in < in.string(),
                        bp::std_out > out.string(), bp::std_err > bp::null,
                        bp::start_dir(cwd.string()));
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        while (child.running() && !cancel && std::chrono::steady_clock::now() < end)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (child.running()) child.terminate();
        child.wait();
        if (cancel) { error = "Canceled"; return false; }
        if (std::chrono::steady_clock::now() >= end) { error = "Process timed out"; return false; }
        if (child.exit_code() != 0) { error = "Process exited without a model"; return false; }
        return true;
    } catch (const std::exception &) { error = "Could not start process"; return false; }
}

std::string read_bounded(const std::filesystem::path &path)
{
    if (!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path) > max_response) return {};
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

bool printable_stl(const std::filesystem::path &mesh, std::string &error)
{
    if (!std::filesystem::is_regular_file(mesh)) { error = "Renderer produced no STL mesh"; return false; }
    const auto length = std::filesystem::file_size(mesh);
    if (length < 134 || length > 100 * 1024 * 1024) { error = "STL size is outside allowed bounds"; return false; }
    std::ifstream stream(mesh, std::ios::binary);
    char header[84];
    stream.read(header, sizeof(header));
    uint32_t count = 0;
    std::memcpy(&count, header + 80, sizeof(count));
    if (count == 0 || static_cast<uint64_t>(count) * 50 + 84 != length) {
        error = "Renderer did not produce a bounded binary STL"; return false;
    }
    bool nondegenerate = false;
    for (uint32_t i = 0; i < count; ++i) {
        char triangle[50];
        stream.read(triangle, sizeof(triangle));
        if (!stream) { error = "STL ended before all triangles"; return false; }
        float vertices[9];
        std::memcpy(vertices, triangle + 12, sizeof(vertices));
        for (int j = 0; j < 9; ++j) {
            if (!std::isfinite(vertices[j]) || std::abs(vertices[j]) > 1000 ||
                (j % 3 == 2 && vertices[j] < -0.01f)) {
                error = "STL has invalid or below-plate coordinates"; return false;
            }
        }
        const float ax = vertices[3] - vertices[0], ay = vertices[4] - vertices[1], az = vertices[5] - vertices[2];
        const float bx = vertices[6] - vertices[0], by = vertices[7] - vertices[1], bz = vertices[8] - vertices[2];
        nondegenerate |= std::abs(ay * bz - az * by) > 0.0001f ||
                         std::abs(az * bx - ax * bz) > 0.0001f ||
                         std::abs(ax * by - ay * bx) > 0.0001f;
    }
    if (!nondegenerate) { error = "STL contains no printable surface"; return false; }
    return true;
}

std::filesystem::path new_revision(const std::filesystem::path &workspace)
{
    std::random_device random;
    for (int i = 0; i < 10; ++i) {
        auto path = workspace / ("revision-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
                                 + "-" + std::to_string(random()));
        if (std::filesystem::create_directory(path)) return path;
    }
    return {};
}
}

bool save_api_key(Provider provider, const std::string &key)
{
#ifdef _WIN32
    const std::string id = target(provider);
    if (id.empty() || key.empty() || key.size() > 4096 || key.find('\0') != std::string::npos) return false;
    const std::wstring name(id.begin(), id.end());
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<wchar_t *>(name.c_str());
    credential.CredentialBlobSize = static_cast<DWORD>(key.size());
    credential.CredentialBlob = reinterpret_cast<BYTE *>(const_cast<char *>(key.data()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    return CredWriteW(&credential, 0) != FALSE;
#else
    return false;
#endif
}

bool clear_api_key(Provider provider)
{
#ifdef _WIN32
    const std::string id = target(provider);
    if (id.empty()) return false;
    const std::wstring name(id.begin(), id.end());
    return CredDeleteW(name.c_str(), CRED_TYPE_GENERIC, 0) != FALSE;
#else
    return false;
#endif
}

bool has_api_key(Provider provider) { return !load_api_key(provider).empty(); }

Result run(const Settings &settings, const std::string &prompt,
           const std::string &revision_note, std::atomic_bool &cancel)
{
    Result result;
    if (settings.model.empty() || settings.model.size() > 120 ||
        prompt.empty() || prompt.size() > 4000 || revision_note.size() > 2000 ||
        settings.timeout_seconds < 5 || settings.timeout_seconds > 300) {
        result.error = "Invalid model, prompt, revision note or timeout"; return result;
    }
    std::filesystem::create_directories(settings.workspace);
    const auto directory = new_revision(settings.workspace);
    if (directory.empty()) { result.error = "Cannot create revision directory"; return result; }
    const std::string request = scene_prompt(prompt, revision_note);
    std::string response;
    if (settings.provider == Provider::AnthropicApi || settings.provider == Provider::OpenAiApi) {
        response = api_request(settings, request, cancel, result.error);
    } else {
        const auto input = directory / "request.txt";
        const auto output = directory / "response.txt";
        { std::ofstream stream(input, std::ios::binary); stream << request; }
        const std::vector<std::string> args = settings.provider == Provider::ClaudeCli ?
            std::vector<std::string>{"-p", "--output-format", "text", "--tools", "", "--model", settings.model} :
            std::vector<std::string>{"exec", "--sandbox", "read-only", "--skip-git-repo-check",
                                     "--model", settings.model, "-"};
        const bool ok = execute(settings.provider_executable, args, input, output, directory,
                                settings.timeout_seconds, cancel, result.error);
        std::filesystem::remove(input);
        if (!ok) return result;
        response = read_bounded(output);
        std::filesystem::remove(output);
    }
    if (!result.error.empty()) return result;
    auto spec = parse_scene(response);
    if (!spec) { result.error = spec.error; return result; }
    const auto source = directory / (settings.renderer == Renderer::OpenSCAD ? "scene.scad" : "scene.py");
    const auto mesh = directory / "preview.stl";
    { std::ofstream stream(source, std::ios::binary);
      stream << (settings.renderer == Renderer::OpenSCAD ? emit_openscad(spec.scene) : emit_blender(spec.scene)); }
    const auto null_input = directory / "empty.txt";
    { std::ofstream stream(null_input); }
    const std::vector<std::string> args = settings.renderer == Renderer::OpenSCAD ?
        std::vector<std::string>{"--export-format", "binstl", "-o", mesh.string(), source.string()} :
        std::vector<std::string>{"--background", "--factory-startup", "--python", source.string(), "--", mesh.string()};
    const bool rendered = execute(settings.renderer_executable, args, null_input, directory / "render.log",
                                  directory, settings.timeout_seconds, cancel, result.error);
    std::filesystem::remove(null_input);
    std::filesystem::remove(directory / "render.log");
    if (!rendered) return result;
    if (!printable_stl(mesh, result.error)) return result;
    if (cancel) { result.error = "Canceled"; return result; }
    const json manifest = {{"version", 1}, {"title", spec.scene.title},
                           {"prompt", prompt}, {"note", revision_note}};
    { std::ofstream stream(directory / "revision.json", std::ios::binary); stream << manifest.dump();
      if (!stream) { result.error = "Could not retain revision metadata"; return result; } }
    result.revision = {prompt, revision_note, std::move(spec.scene), mesh, {}};
    return result;
}

std::vector<Revision> load_revisions(const std::filesystem::path &workspace)
{
    std::vector<Revision> revisions;
    if (!std::filesystem::is_directory(workspace)) return revisions;
    std::vector<std::filesystem::path> directories;
    for (const auto &entry : std::filesystem::directory_iterator(workspace))
        if (entry.is_directory() && entry.path().filename().string().find("revision-") == 0)
            directories.push_back(entry.path());
    std::sort(directories.begin(), directories.end(), [](const auto &a, const auto &b) {
        return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
    });
    if (directories.size() > 100) directories.erase(directories.begin(), directories.end() - 100);
    for (const auto &directory : directories) {
        const auto metadata = json::parse(read_bounded(directory / "revision.json"), nullptr, false);
        if (!metadata.is_object() || !metadata.contains("version") ||
            !metadata["version"].is_number_integer() || metadata["version"] != 1 ||
            !metadata.contains("title") || !metadata["title"].is_string() ||
            !metadata.contains("prompt") || !metadata["prompt"].is_string() ||
            !metadata.contains("note") || !metadata["note"].is_string()) continue;
        const auto title = metadata["title"].get<std::string>();
        const auto prompt = metadata["prompt"].get<std::string>();
        const auto note = metadata["note"].get<std::string>();
        if (title.empty() || title.size() > 80 || prompt.size() > 4000 || note.size() > 2000) continue;
        std::string error;
        const auto mesh = directory / "preview.stl";
        if (!printable_stl(mesh, error)) continue;
        Revision revision;
        revision.scene.title = title;
        revision.prompt = prompt;
        revision.note = note;
        revision.mesh = mesh;
        revisions.push_back(std::move(revision));
    }
    return revisions;
}

} // namespace Slic3r::GUI::ModelCreator
