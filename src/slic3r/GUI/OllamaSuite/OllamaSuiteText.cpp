#include "OllamaSuiteText.hpp"
#include "../I18N.hpp"
#include <algorithm>
#include <cctype>

namespace Slic3r::GUI::OllamaText {
// Named one by one: the application also has a Slic3r::Model, and argument-dependent lookup must not
// mix this namespace's text functions with the core's English ones.
using OllamaSuite::BackendState;
using OllamaSuite::CatalogAttempt;
using OllamaSuite::CatalogFailure;
using OllamaSuite::CatalogSnapshot;
using OllamaSuite::CatalogVerdict;
using OllamaSuite::DestinationProof;
using OllamaSuite::DestinationSource;
using OllamaSuite::Fit;
using OllamaSuite::FitNote;
using OllamaSuite::FitResult;
using OllamaSuite::FitSettings;
using OllamaSuite::Hardware;
using OllamaSuite::KvCache;
using OllamaSuite::Model;
using OllamaSuite::SizeBand;
using OllamaSuite::StoreEntry;
using OllamaSuite::StoreGroup;
using OllamaSuite::StoreQuery;
using OllamaSuite::StoreSort;
using OllamaSuite::StoreState;
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

wxString registration_title()
{
    return _L("Review executable registration");
}

wxString registration_question(const std::string &path, const std::string &sha256)
{
    // TRN: The first %s is the full path of the chosen executable, the second its SHA-256 digest.
    return wxString::Format(_L("Register this executable for the selected local profile?\n\n%s\n\nSHA256: %s\n\nRegistration does not launch it. A separate launch review is required."),
                            u8(path), u8(sha256));
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
    wxString out = wxString::Format(_L("Hardware fit: %s"), verdict_text(fit.verdict)) + "\n";
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
        out += wxString::Format(_L("Hardware measured at %s (UTC)."), u8(fit.measured_at));
    return out;
}

wxString age_text(std::int64_t seconds)
{
    if (seconds < 60)
        return _L("less than a minute");
    if (seconds < 3600) {
        const auto minutes = seconds / 60;
        return minutes == 1 ? _L("1 minute") : wxString::Format(_L("%s minutes"), count(static_cast<std::size_t>(minutes)));
    }
    if (seconds < 48 * 3600) {
        const auto hours = seconds / 3600;
        return hours == 1 ? _L("1 hour") : wxString::Format(_L("%s hours"), count(static_cast<std::size_t>(hours)));
    }
    return wxString::Format(_L("%s days"), count(static_cast<std::size_t>(seconds / 86400)));
}

namespace {
wxString failed_refresh(const CatalogAttempt &attempt)
{
    const wxString at = u8(attempt.attempted_at);
    switch (attempt.failure) {
    case CatalogFailure::Cancelled:
        return wxString::Format(_L("The latest catalog refresh at %s (UTC) was stopped. Only the last verified catalog and the installed models are shown."), at);
    case CatalogFailure::Unavailable:
        return wxString::Format(_L("The latest catalog refresh at %s (UTC) could not reach the official catalog; this computer may be offline. Only the last verified catalog and the installed models are shown."), at);
    case CatalogFailure::Bound:
        return wxString::Format(_L("The latest catalog refresh at %s (UTC) exceeded the safety limit of pages or tags, so nothing from it was kept. Only the last verified catalog and the installed models are shown."), at);
    default:
        return wxString::Format(_L("The latest catalog refresh at %s (UTC) found a page that did not pass validation, so nothing from it was kept. Only the last verified catalog and the installed models are shown."), at);
    }
}
} // namespace

wxString catalog_status(const CatalogSnapshot &s, const std::optional<CatalogAttempt> &attempt, std::int64_t now)
{
    wxString out;
    if (!OllamaSuite::catalog_verified(s)) {
        out = _L("No verified catalog is saved. Installed models remain available. Choose Refresh official catalog to read every family and tag from the official Ollama library.");
    } else {
        const auto pages = count(s.pages.size()), families = count(s.families), tags = count(s.models.size());
        out = s.verdict == CatalogVerdict::Certified
                  ? wxString::Format(_L("Certified complete catalog: every published count reconciled across %s pages (%s families, %s tags)."), pages, families, tags)
                  : wxString::Format(_L("Fully traversed catalog: every family, tag and page link was read across %s pages (%s families, %s tags). The official site publishes no total count, so completeness comes from the traversal and is not certified."), pages, families, tags);
        out += "\n";
        const wxString revision = s.revision.empty() ? _L("not recorded") : u8(s.revision.substr(0, 12));
        out += wxString::Format(_L("Revision %s, verified at %s (UTC)."), revision, u8(s.last_successful_refresh));
        if (const auto age = OllamaSuite::catalog_age(s, now))
            out += " " + (OllamaSuite::catalog_stale(s, now)
                              ? wxString::Format(_L("Stale: verified %s ago. Choose Refresh official catalog to check for new models and tags."), age_text(*age))
                              : wxString::Format(_L("Verified %s ago."), age_text(*age)));
        if (s.cached)
            out += " " + _L("Read from the saved last verified catalog.");
    }
    // A failure newer than the shown catalog is reported; it never replaces the catalog.
    if (attempt && attempt->verdict == CatalogVerdict::Failed) {
        const auto failed = OllamaSuite::utc_seconds(attempt->attempted_at), verified = OllamaSuite::utc_seconds(s.last_successful_refresh);
        if (!verified || (failed && *failed >= *verified))
            out += "\n" + failed_refresh(*attempt);
    }
    return out;
}

wxString catalog_outcome(const CatalogAttempt &attempt)
{
    if (attempt.verdict == CatalogVerdict::Traversed || attempt.verdict == CatalogVerdict::Certified)
        return wxString::Format(_L("Catalog refreshed: %s pages were read and saved as the last verified catalog."), count(attempt.pages));
    return failed_refresh(attempt);
}

wxString store_ui(StoreUi id)
{
    switch (id) {
    case StoreUi::StateFilter: return _L("State");
    case StoreUi::FamilyFilter: return _L("Family");
    case StoreUi::VariantFilter: return _L("Variant");
    case StoreUi::CapabilityFilter: return _L("Capability");
    case StoreUi::QuantizationFilter: return _L("Quantization");
    case StoreUi::SizeFilter: return _L("Size");
    case StoreUi::FitFilter: return _L("Hardware fit");
    case StoreUi::Grouping: return _L("Group");
    case StoreUi::SortOrder: return _L("Sort");
    case StoreUi::ClearFilters: return _L("Clear filters");
    case StoreUi::AnyState: return _L("Any state");
    case StoreUi::AnyFamily: return _L("Any family");
    case StoreUi::AnyVariant: return _L("Any variant");
    case StoreUi::AnyCapability: return _L("Any capability");
    case StoreUi::CapabilitiesUnverified: return _L("Capabilities not verified");
    case StoreUi::AnyQuantization: return _L("Any quantization");
    case StoreUi::QuantizationUnverified: return _L("Quantization not verified");
    case StoreUi::AnySize: return _L("Any size");
    case StoreUi::AnyFit: return _L("Any hardware fit");
    case StoreUi::ChooseFamilyFirst: return _L("Choose a family first; its published variants are then listed here.");
    }
    return wxString();
}

wxString state_text(StoreState state)
{
    switch (state) {
    case StoreState::Running: return _L("Running");
    case StoreState::Installed: return _L("Installed");
    default: return _L("Catalog only");
    }
}

wxString state_explanation(StoreState state)
{
    switch (state) {
    case StoreState::Running: return _L("Running: Ollama has it loaded in memory now, so a chat starts without loading.");
    case StoreState::Installed: return _L("Installed: stored in the model folder. The first chat loads it into memory.");
    default: return _L("Catalog only: published in the official catalog and not installed. Adding it to the batch downloads it; its exact size is read from the official registry when you select it.");
    }
}

wxString size_band_text(SizeBand band)
{
    switch (band) {
    case SizeBand::Under2GiB: return _L("Under 2 GiB");
    case SizeBand::From2To8GiB: return _L("2 to 8 GiB");
    case SizeBand::From8To32GiB: return _L("8 to 32 GiB");
    case SizeBand::Over32GiB: return _L("Over 32 GiB");
    default: return _L("Size not known yet");
    }
}

wxString group_text(StoreGroup group)
{
    switch (group) {
    case StoreGroup::Family: return _L("Group by family");
    case StoreGroup::State: return _L("Group by state");
    case StoreGroup::Fit: return _L("Group by hardware fit");
    case StoreGroup::Quantization: return _L("Group by quantization");
    case StoreGroup::Size: return _L("Group by size");
    default: return _L("No grouping");
    }
}

wxString sort_text(StoreSort sort)
{
    switch (sort) {
    case StoreSort::Family: return _L("Sort by family");
    case StoreSort::SizeAscending: return _L("Sort by size, smallest first");
    case StoreSort::SizeDescending: return _L("Sort by size, largest first");
    case StoreSort::Fit: return _L("Sort by hardware fit, best first");
    default: return _L("Sort by name");
    }
}

wxString verdict_explanation(Fit verdict)
{
    switch (verdict) {
    case Fit::RunsWell: return _L("Runs well: the estimate fits in GPU memory that Ollama has shown it can use.");
    case Fit::WithLimits: return _L("Runs with limits: the estimate fits only with the processor doing part or all of the work, so replies are slower.");
    case Fit::Unlikely: return _L("Unlikely: the model folder or the measured memory cannot hold the estimate.");
    default: return _L("Unknown: evidence is missing, so no estimate is made. Inspect the model, refresh the runtime or measure hardware again.");
    }
}

wxString capability_explanation(const std::string &capability)
{
    if (capability == "completion") return _L("completion: writes text replies. Chat needs it.");
    if (capability == "vision") return _L("vision: accepts images with a message.");
    if (capability == "tools") return _L("tools: can ask a client to run functions that the client defines.");
    if (capability == "embedding") return _L("embedding: turns text into vectors for search; it does not chat.");
    if (capability == "thinking") return _L("thinking: can return its reasoning separately before the answer.");
    if (capability == "insert") return _L("insert: can fill in text between a given beginning and end.");
    return wxString::Format(_L("%s: reported by Ollama; no explanation is bundled for it."), u8(capability));
}

wxString quantization_explanation(const std::string &quantization)
{
    if (quantization.empty())
        return _L("Quantization is verified only from installed-model metadata.");
    std::string upper = quantization;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (upper.rfind("F32", 0) == 0)
        return _L("32-bit weights: the original precision and the largest size.");
    if (upper.rfind("F16", 0) == 0 || upper.rfind("BF16", 0) == 0)
        return _L("16-bit weights: close to the original quality, with the largest memory and storage needs of the common variants.");
    // The label names its bit width (Q4_K_M, IQ3_XS, Q8_0): read it, never the model name.
    const auto digit = upper.find_first_of("0123456789");
    if ((upper.rfind("Q", 0) == 0 || upper.rfind("IQ", 0) == 0) && digit != std::string::npos && digit <= 2) {
        const int bits = upper[digit] - '0';
        if (bits >= 1 && bits <= 8)
            return wxString::Format(_L("About %s bits per weight. Fewer bits need less memory and storage but lose more quality; 4 to 5 bits is a common balance."),
                                    count(static_cast<std::size_t>(bits)));
    }
    return _L("A quantization reported by Ollama; no explanation is bundled for this label.");
}

wxString store_row(const StoreEntry &entry)
{
    const auto &m = entry.model;
    wxString row = u8(m.name) + " | " + state_text(OllamaSuite::store_state(m));
    if (!m.quantization.empty())
        row += " | " + u8(m.quantization);
    if (m.bytes)
        row += " | " + bytes(*m.bytes);
    row += " | " + verdict_text(entry.fit);
    return row;
}

namespace {
wxString group_label(StoreGroup group, const StoreEntry &first)
{
    switch (group) {
    case StoreGroup::Family: return u8(OllamaSuite::model_family(first.model.name));
    case StoreGroup::State: return state_text(OllamaSuite::store_state(first.model));
    case StoreGroup::Fit: return verdict_text(first.fit);
    case StoreGroup::Quantization: return first.model.quantization.empty() ? store_ui(StoreUi::QuantizationUnverified) : u8(first.model.quantization);
    case StoreGroup::Size: return size_band_text(OllamaSuite::size_band(first.model));
    default: return wxString();
    }
}
wxString variants_text(std::size_t n) { return n == 1 ? _L("1 variant") : wxString::Format(_L("%s variants"), count(n)); }
wxString family_explanation(const std::string &family)
{
    return wxString::Format(_L("Family %s: the variants published under one model name. They differ in parameter count, quantization or tuning, and each is pulled by its exact tag."), u8(family));
}
wxString size_explanation(SizeBand band)
{
    if (band == SizeBand::Unknown)
        return _L("Size not known yet: the exact size is read from the official registry when you select a catalog entry.");
    return wxString::Format(_L("%s: each variant needs about this much space in the model folder, plus a ten percent allowance while it downloads."), size_band_text(band));
}
} // namespace

wxString group_heading(StoreGroup group, const StoreEntry &first, std::size_t members)
{
    return wxString::Format(_L("Group: %s, %s"), group_label(group, first), variants_text(members));
}

wxString group_explanation(StoreGroup group, const StoreEntry &first, std::size_t members)
{
    wxString out = group_heading(group, first, members) + "\n";
    switch (group) {
    case StoreGroup::Family: out += family_explanation(OllamaSuite::model_family(first.model.name)); break;
    case StoreGroup::State: out += state_explanation(OllamaSuite::store_state(first.model)); break;
    case StoreGroup::Fit: out += verdict_explanation(first.fit); break;
    case StoreGroup::Quantization: out += quantization_explanation(first.model.quantization); break;
    case StoreGroup::Size: out += size_explanation(OllamaSuite::size_band(first.model)); break;
    default: break;
    }
    return out + "\n" + _L("Select a variant under this heading to see its details.");
}

wxString model_explanation(const Model &m, Fit verdict)
{
    wxString out = u8(m.name) + "\n";
    out += family_explanation(OllamaSuite::model_family(m.name)) + "\n";
    out += wxString::Format(_L("Variant %s: the exact tag that is pulled and run."), u8(OllamaSuite::model_variant(m.name))) + "\n";
    out += state_explanation(OllamaSuite::store_state(m)) + "\n";
    if (m.capabilities_verified) {
        wxString list;
        for (const auto &capability : m.capabilities)
            list += (list.empty() ? wxString() : wxString(", ")) + u8(capability);
        out += wxString::Format(_L("Verified capabilities: %s"), list.empty() ? _L("none reported") : list) + "\n";
        for (const auto &capability : m.capabilities)
            out += "- " + capability_explanation(capability) + "\n";
    } else
        out += _L("Capabilities are verified only after the model is installed and inspected.") + "\n";
    out += (m.quantization.empty() ? quantization_explanation(m.quantization)
                                   : wxString::Format(_L("Quantization %s: %s"), u8(m.quantization), quantization_explanation(m.quantization))) + "\n";
    if (m.bytes && m.installed)
        out += wxString::Format(_L("Uses %s in the model folder."), bytes(*m.bytes)) + "\n";
    else if (m.bytes)
        out += wxString::Format(_L("Downloads %s; with the ten percent allowance it needs %s free in the model folder."), bytes(*m.bytes), bytes(*m.bytes + *m.bytes / 10)) + "\n";
    else
        out += size_explanation(SizeBand::Unknown) + "\n";
    out += wxString::Format(_L("Hardware fit now: %s"), verdict_explanation(verdict));
    return out;
}

wxString query_summary(const StoreQuery &q, std::size_t shown, std::size_t total)
{
    wxString out = wxString::Format(_L("Showing %s of %s variants."), count(shown), count(total));
    if (q.state)
        out += "\n" + state_explanation(*q.state);
    if (q.family)
        out += "\n" + family_explanation(*q.family);
    if (q.variant)
        out += "\n" + wxString::Format(_L("Variant %s: the exact tag that is pulled and run."), u8(*q.variant));
    if (q.capability)
        out += "\n" + (q.capability->empty() ? _L("Capabilities are verified only after the model is installed and inspected.") : capability_explanation(*q.capability));
    if (q.quantization)
        out += "\n" + quantization_explanation(*q.quantization);
    if (q.size)
        out += "\n" + size_explanation(*q.size);
    if (q.fit)
        out += "\n" + verdict_explanation(*q.fit);
    return out;
}
} // namespace Slic3r::GUI::OllamaText
