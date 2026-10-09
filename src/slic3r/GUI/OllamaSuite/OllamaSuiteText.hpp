#pragma once
#include "libslic3r/OllamaSuite/OllamaCore.hpp"
#include <wx/string.h>

// Localized presentation of the suite's structured evidence. The core reports
// facts and reason codes; every sentence a person reads is built here so it
// passes through the translation catalog.
namespace Slic3r::GUI::OllamaText {
// Labels and notices of the Models section's evidence and estimate controls.
enum class Ui { HardwareEvidence, MeasureAgain, EstimateContext, CachePrecision, SettingsNotSaved, InspectionFinished, MeasuredAgain };
wxString ui(Ui id);
wxString bytes(std::uint64_t value);
wxString verdict_text(OllamaSuite::Fit verdict);
wxString note_text(OllamaSuite::FitNote note);
wxString destination_source(OllamaSuite::DestinationSource source);
wxString kv_cache(OllamaSuite::KvCache cache);
wxString context_choice(std::uint64_t tokens);
wxString fit_settings_help();
wxString hardware_summary(const OllamaSuite::Hardware &hardware);
wxString fit_report(const OllamaSuite::Model &model, const OllamaSuite::FitResult &fit);
wxString age_text(std::int64_t seconds);
// Verdict, page count, revision, verification time, age or staleness of the shown catalog, and
// whether the latest refresh failed so that only the last verified catalog is shown.
wxString catalog_status(const OllamaSuite::CatalogSnapshot &catalog, const std::optional<OllamaSuite::CatalogAttempt> &attempt, std::int64_t now);
// One-line outcome of the refresh that just finished.
wxString catalog_outcome(const OllamaSuite::CatalogAttempt &attempt);

// Model Store filters, grouping, sorting and the explanations shown before a model is chosen.
enum class StoreUi {
    StateFilter, FamilyFilter, VariantFilter, CapabilityFilter, QuantizationFilter, SizeFilter, FitFilter, Grouping, SortOrder,
    ClearFilters, AnyState, AnyFamily, AnyVariant, AnyCapability, CapabilitiesUnverified, AnyQuantization,
    QuantizationUnverified, AnySize, AnyFit, ChooseFamilyFirst
};
wxString store_ui(StoreUi id);
wxString state_text(OllamaSuite::StoreState state);
wxString state_explanation(OllamaSuite::StoreState state);
wxString size_band_text(OllamaSuite::SizeBand band);
wxString group_text(OllamaSuite::StoreGroup group);
wxString sort_text(OllamaSuite::StoreSort sort);
wxString verdict_explanation(OllamaSuite::Fit verdict);
wxString capability_explanation(const std::string &capability);
wxString quantization_explanation(const std::string &quantization);
wxString store_row(const OllamaSuite::StoreEntry &entry);
wxString group_heading(OllamaSuite::StoreGroup group, const OllamaSuite::StoreEntry &first, std::size_t members);
wxString group_explanation(OllamaSuite::StoreGroup group, const OllamaSuite::StoreEntry &first, std::size_t members);
// What the family, variant, state, capabilities, quantization, size, storage and current fit mean for this model.
wxString model_explanation(const OllamaSuite::Model &model, OllamaSuite::Fit verdict);
wxString query_summary(const OllamaSuite::StoreQuery &query, std::size_t shown, std::size_t total);
} // namespace Slic3r::GUI::OllamaText
