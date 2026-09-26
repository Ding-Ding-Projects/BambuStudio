#pragma once

#include "SceneSpec.hpp"

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

namespace Slic3r::GUI::ModelCreator {

enum class Provider { ClaudeCli, CodexCli, AnthropicApi, OpenAiApi };
enum class Renderer { OpenSCAD, Blender };

struct Settings {
    Provider provider = Provider::ClaudeCli;
    Renderer renderer = Renderer::OpenSCAD;
    std::string model;
    std::filesystem::path provider_executable;
    std::filesystem::path renderer_executable;
    std::filesystem::path workspace;
    int timeout_seconds = 90;
};

struct Revision {
    std::string prompt;
    std::string note;
    SceneSpec scene;
    std::filesystem::path mesh;
    std::string warning;
};

struct Result {
    Revision revision;
    std::string error;
    explicit operator bool() const { return error.empty(); }
};

// Keys live in the Windows Credential Manager. No secret, prompt, provider
// response or generated scene is placed in the diagnostic path.
bool save_api_key(Provider provider, const std::string &key);
bool clear_api_key(Provider provider);
bool has_api_key(Provider provider);

// run() does not add to the plate. Caller must explicitly confirm that action.
// It creates a new isolated revision directory and retains prior revisions.
Result run(const Settings &settings, const std::string &prompt,
           const std::string &revision_note, std::atomic_bool &cancel);
std::vector<Revision> load_revisions(const std::filesystem::path &workspace);

} // namespace Slic3r::GUI::ModelCreator
