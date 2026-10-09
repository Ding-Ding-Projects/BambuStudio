#include "OllamaSuiteText.hpp"
#include "../I18N.hpp"
#include <algorithm>

namespace Slic3r::GUI::OllamaText {
// Named one by one: the application also has a Slic3r::Model, and this namespace has its own fit_label.
using OllamaSuite::BackendState;
using OllamaSuite::DestinationProof;
using OllamaSuite::DestinationSource;
using OllamaSuite::Fit;
using OllamaSuite::FitNote;
using OllamaSuite::FitResult;
using OllamaSuite::FitSettings;
using OllamaSuite::Hardware;
using OllamaSuite::KvCache;
using OllamaSuite::Model;
namespace {
wxString u8(const std::string &s) { return wxString::FromUTF8(s.c_str()); }
wxString count(std::size_t n) { return wxString::Format("%llu", static_cast<unsigned long long>(n)); }
wxString path_text(const std::filesystem::path &p) { return wxString(p.wstring()); }
wxString free_text(const std::optional<std::uint64_t> &value) { return value ? bytes(*value) : _L("not measurable"); }
} // namespace

wxString ui(Ui id)
{
    switch (id) {
    case Ui::HardwareEvidence: return _L("Measured hardware evidence");
    case Ui::MeasureAgain: return _L("Measure hardware again");
    case Ui::EstimateContext: return _L("Context for estimates");
    case Ui::CachePrecision: return _L("Context cache precision");
    case Ui::SettingsNotSaved: return _L("Estimate settings could not be saved. They apply until the suite closes.");
    case Ui::InspectionFinished: return _L("Inspection finished. Hardware fit was recomputed from current measurements and estimate settings.");
    case Ui::MeasuredAgain: return _L("Hardware measured again. Select a model to see its verdict.");
    }
    return wxString();
}

wxString bytes(std::uint64_t value)
{
    const double gib = static_cast<double>(value) / (1024.0 * 1024.0 * 1024.0);
    if (gib >= 1.0)
        return wxString::Format("%.1f GiB", gib);
    return wxString::Format("%.0f MiB", static_cast<double>(value) / (1024.0 * 1024.0));
}

wxString verdict_text(Fit verdict)
{
    switch (verdict) {
    case Fit::RunsWell: return _L("Runs well");
    case Fit::WithLimits: return _L("Runs with limits");
    case Fit::Unlikely: return _L("Unlikely");
    default: return _L("Unknown");
    }
}

wxString note_text(FitNote note)
{
    switch (note) {
    case FitNote::Conservative: return _L("Estimates are conservative and do not guarantee that the model will run.");
    case FitNote::MissingSizeOrDestination: return _L("The exact model size, the free space in the model folder, or the measurement time is unavailable.");
    case FitNote::StorageOverflow: return _L("The storage estimate is too large to calculate.");
    case FitNote::InsufficientDisk: return _L("The model folder has less free space than the model size plus a ten percent safety allowance.");
    case FitNote::MissingMemoryEvidence: return _L("Measured system memory, processor architecture and the model's parameters, quantization, context limit and attention layout are all needed. Inspect an installed model to read them.");
    case FitNote::ContextExceedsModel: return _L("The estimate context is larger than the model's verified context limit.");
    case FitNote::MemoryOverflow: return _L("The memory estimate is too large to calculate.");
    case FitNote::MemoryFormula: return _L("Memory estimate: model size, plus twenty percent for the runtime, plus the context cache for the chosen context and precision.");
    case FitNote::PartialOffload: return _L("The model does not fit in usable GPU memory, so part of it runs on the processor. Speed is not predicted.");
    case FitNote::NoMemoryFits: return _L("Neither measured system memory nor usable GPU memory holds this estimate.");
    case FitNote::GpuUnverifiedCpuEstimate: return _L("Ollama has not shown a working GPU backend on this computer, so the verdict assumes processor-only inference.");
    case FitNote::GpuUnverified: return _L("System memory alone is not enough, and Ollama has not shown a working GPU backend, so GPU use cannot be counted.");
    case FitNote::ContextLimitedToModel: return _L("The estimate context was reduced to the model's verified context limit.");
    }
    return wxString();
}

wxString destination_source(DestinationSource source)
{
    switch (source) {
    case DestinationSource::ProcessSetting: return _L("OLLAMA_MODELS in this application's environment");
    case DestinationSource::UserSetting: return _L("OLLAMA_MODELS user setting");
    case DestinationSource::MachineSetting: return _L("OLLAMA_MODELS computer setting");
    default: return _L("Ollama default location");
    }
}

wxString kv_cache(KvCache cache)
{
    switch (cache) {
    case KvCache::Q8_0: return _L("8-bit context cache (OLLAMA_KV_CACHE_TYPE=q8_0)");
    case KvCache::Q4_0: return _L("4-bit context cache (OLLAMA_KV_CACHE_TYPE=q4_0)");
    default: return _L("16-bit context cache (default, recommended)");
    }
}

wxString context_choice(std::uint64_t tokens)
{
    const FitSettings recommended;
    return tokens == recommended.context ? wxString::Format(_L("%s tokens (recommended)"), count(tokens))
                                         : wxString::Format(_L("%s tokens"), count(tokens));
}

wxString fit_settings_help()
{
    return _L("Context memory grows with the number of tokens a chat keeps and with the cache precision Ollama uses. "
              "4096 tokens with a 16-bit cache is a conservative starting point. Choose the context and "
              "OLLAMA_KV_CACHE_TYPE you run Ollama with; every verdict is recomputed at once.");
}

wxString hardware_summary(const Hardware &h)
{
    if (h.measured_at.empty())
        return _L("Hardware has not been measured yet. Choose Refresh runtime or Measure hardware again.");
    wxString out = wxString::Format(_L("Measured at %s (UTC)."), u8(h.measured_at));
    out += "\n";
    if (h.available_ram && h.total_ram)
        out += wxString::Format(_L("System memory available: %s of %s."), bytes(*h.available_ram), bytes(*h.total_ram));
    else if (h.available_ram)
        out += wxString::Format(_L("System memory available: %s."), bytes(*h.available_ram));
    else
        out += _L("System memory: not measured on this platform.");
    out += "\n";
    out += h.architecture.empty() ? _L("Processor architecture: not identified.")
                                  : wxString::Format(_L("Processor architecture: %s."), u8(h.architecture));
    out += "\n";
    if (h.gpus.empty())
        out += _L("Graphics: no hardware graphics adapter was reported.") + "\n";
    for (const auto &gpu : h.gpus) {
        const wxString driver = gpu.driver_version.empty() ? _L("driver version unavailable")
                                                           : wxString::Format(_L("driver %s"), u8(gpu.driver_version));
        const auto usable = OllamaSuite::usable_vram({gpu});
        out += wxString::Format(_L("Graphics: %s, %s, %s dedicated memory, %s usable now."), u8(gpu.name), driver,
                                bytes(gpu.dedicated_bytes), usable ? bytes(*usable) : _L("none")) + "\n";
    }
    const auto &seen = h.backend_observation;
    switch (h.backend_state) {
    case BackendState::Gpu:
        out += wxString::Format(_L("GPU backend: verified. Ollama %s kept %s of %s for %s in GPU memory at %s (UTC)."),
                                u8(seen->runtime_version), bytes(seen->size_vram), bytes(seen->size), u8(seen->model), u8(seen->observed_at));
        break;
    case BackendState::CpuOnly:
        out += wxString::Format(_L("GPU backend: not in use. Ollama %s kept %s entirely in system memory at %s (UTC), so estimates assume processor inference."),
                                u8(seen->runtime_version), u8(seen->model), u8(seen->observed_at));
        break;
    case BackendState::Changed:
        out += _L("GPU backend: the earlier Ollama evidence no longer applies because the graphics adapter, driver or Ollama version changed. Load a model in Ollama again, then choose Refresh runtime.");
        break;
    default:
        out += _L("GPU backend: not verified. Ollama reports GPU use only while a model is loaded. Send one chat message or load a model in Ollama, then choose Refresh runtime.");
        break;
    }
    out += "\n";
    const auto &d = h.destination;
    switch (d.proof) {
    case DestinationProof::Manifests:
        out += wxString::Format(_L("Model folder: %s (%s). Confirmed by %s of %s installed model manifests. Free space: %s."),
                                path_text(d.path), destination_source(d.source), count(d.manifests_found), count(d.manifests_checked), free_text(d.free_bytes));
        break;
    case DestinationProof::Configured:
        out += h.inventory_known
                   ? wxString::Format(_L("Model folder: %s (%s). No installed model exists yet to confirm it. Free space: %s."),
                                      path_text(d.path), destination_source(d.source), free_text(d.free_bytes))
                   : wxString::Format(_L("Model folder: %s (%s), not yet confirmed because the installed-model list has not been read. Choose Refresh runtime to confirm it. Free space: %s."),
                                      path_text(d.path), destination_source(d.source), free_text(d.free_bytes));
        if (d.candidates_disagree)
            out += " " + _L("The documented settings name different folders, so the smallest free space among them is used.");
        break;
    default:
        if (d.manifests_checked > 0)
            out += wxString::Format(_L("Model folder: not confirmed. The installed models were not found under any documented location (closest: %s, %s of %s manifests). Storage fit stays Unknown. Check OLLAMA_MODELS for the running Ollama, restart Ollama, then choose Refresh runtime."),
                                    path_text(d.path), count(d.manifests_found), count(d.manifests_checked));
        else
            out += _L("Model folder: no documented location could be resolved, so storage fit stays Unknown.");
        break;
    }
    return out;
}

wxString fit_report(const Model &model, const FitResult &fit)
{
    wxString out = u8(model.name) + "\n";
    out += wxString::Format(_L("Hardware fit: %s"), verdict_text(fit.verdict)) + "\n";
    if (model.bytes)
        out += wxString::Format(model.installed ? _L("Installed size: %s.") : _L("Exact registry download size: %s."), bytes(*model.bytes)) + "\n";
    if (!model.installed && !model.digest.empty())
        out += wxString::Format(_L("Manifest: %s"), u8(model.digest)) + "\n";
    if (fit.disk_required)
        out += (*fit.disk_required == 0 ? _L("Storage needed: none, the model is already installed.")
                                        : wxString::Format(_L("Storage needed: %s including a ten percent allowance."), bytes(*fit.disk_required))) + "\n";
    if (fit.context > 0)
        out += (fit.context_bytes ? wxString::Format(_L("Estimate context: %s tokens, needing %s of context cache."), count(fit.context), bytes(*fit.context_bytes))
                                  : wxString::Format(_L("Estimate context: %s tokens. The context cache size is unknown because attention metadata is missing."), count(fit.context))) + "\n";
    if (fit.memory_required)
        out += wxString::Format(_L("Memory estimate: %s."), bytes(*fit.memory_required)) + "\n";
    for (const auto note : fit.notes)
        out += "- " + note_text(note) + "\n";
    if (!fit.measured_at.empty())
        out += wxString::Format(_L("Hardware measured at %s (UTC)."), u8(fit.measured_at)) + "\n";
    if (model.capabilities_verified) {
        wxString list;
        for (const auto &capability : model.capabilities)
            list += (list.empty() ? wxString() : wxString(", ")) + u8(capability);
        out += wxString::Format(_L("Verified capabilities: %s"), list.empty() ? _L("none reported") : list);
    } else
        out += _L("Capabilities are verified only after the model is installed and inspected.");
    return out;
}
} // namespace Slic3r::GUI::OllamaText
