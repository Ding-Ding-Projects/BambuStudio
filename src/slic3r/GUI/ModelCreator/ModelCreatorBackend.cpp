#include "ModelCreatorBackend.hpp"
#include "ProcessRunner.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <random>

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#endif

namespace Slic3r::GUI::ModelCreator {
namespace {
using json = nlohmann::json;
constexpr size_t max_response = 65536;

struct TemporaryFiles {
    std::vector<std::filesystem::path> paths;
    ~TemporaryFiles()
    {
        for (const auto &path : paths) {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    }
};

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
    const json schema = json::parse(scene_json_schema());
    json body = anthropic ?
        json{{"model", settings.model}, {"max_tokens", 4096},
             {"messages", json::array({{{"role", "user"}, {"content", prompt}}})},
             {"output_config", {{"format", {{"type", "json_schema"}, {"schema", schema}}}}}} :
        json{{"model", settings.model}, {"input", prompt}, {"max_output_tokens", 4096},
             {"text", {{"format", {{"type", "json_schema"}, {"name", "model_creator_scene_v1"},
                                    {"strict", true}, {"schema", schema}}}}}};
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
             ProcessRole role, std::string &error)
{
    return run_isolated_process(program, args, in, out, cwd, seconds, cancel, role, error);
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

void remove_stale_temporary_files(const std::filesystem::path &directory)
{
    // An abrupt application exit skips RAII cleanup. Reap only old scratch files
    // so another open dialog's active generation is not disturbed.
    const auto cutoff = std::filesystem::file_time_type::clock::now() - std::chrono::hours(24);
    for (const char *name : {"request.txt", "response.txt", "schema.json", "process-output.txt",
                             "empty.txt", "render.log"}) {
        const auto path = directory / name;
        std::error_code error;
        const auto written = std::filesystem::last_write_time(path, error);
        if (!error && written < cutoff) std::filesystem::remove(path, error);
    }
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

bool has_api_key(Provider provider)
{
#ifdef _WIN32
    const std::string id = target(provider);
    if (id.empty()) return false;
    const std::wstring name(id.begin(), id.end());
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(name.c_str(), CRED_TYPE_GENERIC, 0, &credential)) return false;
    const bool available = credential->CredentialBlobSize > 0;
    CredFree(credential);
    return available;
#else
    return false;
#endif
}

bool test_api_key(Provider provider, std::string &error)
{
    if (provider != Provider::AnthropicApi && provider != Provider::OpenAiApi) {
        error = "Choose an API provider";
        return false;
    }
    std::string key = load_api_key(provider);
    if (key.empty()) { error = "No key is saved for this provider"; return false; }
    CURL *curl = curl_easy_init();
    if (!curl) { error = "HTTP client unavailable"; return false; }
    curl_slist *headers = nullptr;
    const bool anthropic = provider == Provider::AnthropicApi;
    const std::string authorization = anthropic ? "x-api-key: " + key : "Authorization: Bearer " + key;
    headers = curl_slist_append(headers, authorization.c_str());
    if (anthropic) headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    curl_easy_setopt(curl, CURLOPT_URL, anthropic ? "https://api.anthropic.com/v1/models?limit=1" :
                                                  "https://api.openai.com/v1/models?limit=1");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                     +[](char *, size_t size, size_t count, void *) -> size_t { return size * count; });
    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    std::fill(key.begin(), key.end(), '\0');
    if (result != CURLE_OK) {
        error = "Provider connection could not be completed";
        return false;
    }
    if (status < 200 || status >= 300) {
        error = "Provider key test returned HTTP " + std::to_string(status);
        return false;
    }
    return true;
}

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
    const auto generate_once = [&](const std::string &request) -> std::string {
        if (settings.provider == Provider::AnthropicApi || settings.provider == Provider::OpenAiApi)
            return api_request(settings, request, cancel, result.error);
        const auto input = directory / "request.txt";
        const auto output = directory / "response.txt";
        const auto schema_file = directory / "schema.json";
        const auto trace = directory / "process-output.txt";
        TemporaryFiles temporary{{input, output, schema_file, trace}};
        { std::ofstream stream(input, std::ios::binary); stream << request; }
        { std::ofstream stream(schema_file, std::ios::binary); stream << scene_json_schema(); }
        const std::vector<std::string> args = settings.provider == Provider::ClaudeCli ?
            std::vector<std::string>{"-p", "--safe-mode", "--strict-mcp-config", "--mcp-config", "{}",
                                     "--no-session-persistence", "--output-format", "json",
                                     "--json-schema", scene_json_schema(), "--tools", "",
                                     "--model", settings.model} :
            std::vector<std::string>{"exec", "--ignore-user-config", "--ignore-rules", "--ephemeral",
                                     "--sandbox", "read-only", "--skip-git-repo-check",
                                     "--disable", "apps", "--disable", "browser_use",
                                     "--disable", "computer_use", "--disable", "hooks",
                                     "--disable", "plugins", "--disable", "multi_agent",
                                     "--output-schema", schema_file.string(), "-o", output.string(),
                                     "--model", settings.model, "-"};
        const auto role = settings.provider == Provider::ClaudeCli ? ProcessRole::ClaudeCli : ProcessRole::CodexCli;
        if (!execute(settings.provider_executable, args, input,
                     settings.provider == Provider::ClaudeCli ? output : trace, directory,
                     settings.timeout_seconds, cancel, role, result.error)) return {};
        std::string response = read_bounded(output);
        if (settings.provider != Provider::ClaudeCli) return response;
        const auto envelope = json::parse(response, nullptr, false);
        if (!envelope.is_object() || !envelope.contains("structured_output") ||
            !envelope["structured_output"].is_object()) {
            result.error = "Claude CLI returned no structured scene";
            return {};
        }
        return envelope["structured_output"].dump();
    };
    ParseResult spec;
    std::string request = scene_prompt(prompt, revision_note);
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (cancel) { result.error = "Canceled"; return result; }
        const std::string response = generate_once(request);
        if (!result.error.empty()) return result;
        spec = parse_scene(response);
        if (spec) break;
        if (attempt == 2) { result.error = spec.error; return result; }
        request = scene_prompt(prompt, revision_note) +
                  " The previous scene did not pass validation: " + spec.error.substr(0, 160) +
                  ". Return a complete corrected JSON object matching the schema.";
    }
    const auto source = directory / (settings.renderer == Renderer::OpenSCAD ? "scene.scad" : "scene.py");
    const auto mesh = directory / "preview.stl";
    const auto editable_blend = directory / "scene.blend";
    { std::ofstream stream(source, std::ios::binary);
      stream << (settings.renderer == Renderer::OpenSCAD ? emit_openscad(spec.scene) : emit_blender(spec.scene)); }
    const auto null_input = directory / "empty.txt";
    TemporaryFiles temporary{{null_input, directory / "render.log"}};
    { std::ofstream stream(null_input); }
    const std::vector<std::string> args = settings.renderer == Renderer::OpenSCAD ?
        std::vector<std::string>{"--export-format", "binstl", "-o", mesh.string(), source.string()} :
        std::vector<std::string>{"--background", "--factory-startup", "--python", source.string(), "--",
                                 editable_blend.string(), mesh.string()};
    const bool rendered = execute(settings.renderer_executable, args, null_input, directory / "render.log",
                                  directory, settings.timeout_seconds, cancel, ProcessRole::Renderer, result.error);
    if (!rendered) return result;
    if (settings.renderer == Renderer::Blender && !std::filesystem::is_regular_file(editable_blend)) {
        result.error = "Renderer produced no editable Blender project"; return result;
    }
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
    for (const auto &directory : directories) remove_stale_temporary_files(directory);
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
