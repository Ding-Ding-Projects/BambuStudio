#pragma once
#include "libslic3r/OllamaSuite/OllamaCore.hpp"
#include <wx/string.h>

// Localized presentation of the suite's structured evidence. The core reports
// facts and reason codes; every sentence a person reads is built here so it
// passes through the translation catalog.
namespace Slic3r::GUI::OllamaText {
wxString bytes(std::uint64_t value);
wxString verdict_text(OllamaSuite::Fit verdict);
wxString note_text(OllamaSuite::FitNote note);
wxString destination_source(OllamaSuite::DestinationSource source);
wxString kv_cache(OllamaSuite::KvCache cache);
wxString context_choice(std::uint64_t tokens);
wxString fit_settings_help();
wxString hardware_summary(const OllamaSuite::Hardware &hardware);
wxString fit_report(const OllamaSuite::Model &model, const OllamaSuite::FitResult &fit);
} // namespace Slic3r::GUI::OllamaText
