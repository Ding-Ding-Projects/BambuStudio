#pragma once
#include "libslic3r/OllamaSuite/OllamaCore.hpp"

namespace Slic3r::OllamaSuite {
enum class RuntimeState { Healthy, Unreachable, Unhealthy, Cancelled };
struct ApiResult { RuntimeState state = RuntimeState::Unhealthy; Json value; long http_status = 0; std::string diagnostic; bool terminal = false; };
// Every local operation is fixed to numerical IPv4 loopback, bypasses proxy discovery,
// disallows redirects and sends no ambient application headers or credentials.
class LocalClient {
public:
    ApiResult execute(Operation, const Json &, const std::atomic_bool &cancel,
                      const std::function<bool(const Json &)> &on_chunk = {}) const;
    CatalogPage catalog_page(const std::string &, const std::atomic_bool &cancel) const;
    Model registry_metadata(const std::string &exact_tag, const std::atomic_bool &cancel) const;
};
// The documented views of OLLAMA_MODELS: the persisted user/computer setting (or the default location)
// and this process's environment (or the default location). Duplicates are removed.
std::vector<DestinationCandidate> destination_candidates();
// Measures this computer now: RAM, architecture, hardware graphics adapters with driver version,
// dedicated memory and the operating system's current budget, and the model destination proven by
// the installed models the local API reported. Backend evidence is applied separately by apply_backend.
Hardware detect_hardware(const std::vector<Model> &installed);
}
