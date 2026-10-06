// Native integration test. Requires a real wxWidgets display and the production
// SurfaceRegistry, ElementStyle, I18N and PersonalVocabulary implementations.
#include "slic3r/GUI/FeatureServices/SurfaceRegistry.hpp"
#include "slic3r/GUI/Appearance/ElementStyle.hpp"
#include <wx/app.h>
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <cstdio>
#include <stdexcept>

using Slic3r::GUI::FeatureServices::SurfaceRegistry;
class RegistryTestApp : public wxApp { public: bool OnInit() override { return true; } };
wxIMPLEMENT_APP_NO_MAIN(RegistryTestApp);

static void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char **argv)
{
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) return 2;
    auto &registry = SurfaceRegistry::instance();
    auto *frame = new wxFrame(nullptr, wxID_ANY, "Registry integration");
    int result = 0;
    try {
        require(!registry.capture_permitted(frame), "unregistered capture must be rejected");
        require(!registry.history_permitted(frame), "unregistered history must be rejected");
        require(!registry.export_permitted(frame), "unregistered export must be rejected");
        require(registry.register_surface(frame, "registry-test"), "root registration");
        auto *child = new wxPanel(frame);
        require(registry.capture_permitted(child), "dynamic child discovery before idle");
        const auto id = registry.surface_id(child);
        require(!id.empty(), "dynamic identity");
        require(Slic3r::GUI::ElementStyle::element_id_of(child) == id, "real appearance adoption");
        delete child;
        auto *replacement = new wxPanel(frame);
        require(registry.surface_id(replacement) == id, "replacement reuses structural slot");
        require(registry.register_surface(replacement, "registry-test/details"), "explicit ID replaces discovered ID");
        auto *duplicate = new wxPanel(frame);
        require(!registry.register_surface(duplicate, "registry-test/details"), "duplicate live identity rejected");

        auto *label = new wxStaticText(replacement, wxID_ANY, "Initial label");
        registry.synchronize();
        registry.record_label(label, "Original message");
        const auto first = label->GetLabel();
        label->SetLabel("Changed rendered label");
        registry.refresh_presentation();
        require(label->GetLabel() == first, "refresh uses recorded source, never current rendering");
        require(registry.record_label_renderer(label, "Factual source", [](const wxString &source) {
            return wxString("Exact: ") + source;
        }), "explicit factual renderer registration");
        require(label->GetLabel() == "Exact: Factual source", "provided renderer owns final text");
        label->SetLabel("Unrelated displayed wording");
        registry.refresh_presentation();
        require(label->GetLabel() == "Exact: Factual source", "factual renderer receives original source on refresh");
        require(!registry.record_label_renderer(label, "Rejected replacement", {}), "empty renderer rejected");
        require(label->GetLabel() == "Exact: Factual source", "invalid renderer leaves existing presentation unchanged");
        registry.record_label(label, "Original message");
        require(label->GetLabel() == first, "normal registration clears earlier factual renderer");
        registry.clear_label_source(label);
        label->SetLabel("2030-12-31 12:34 / code 42");
        registry.refresh_presentation();
        require(label->GetLabel() == "2030-12-31 12:34 / code 42", "dynamic label source opt-out");
        auto *dynamic = new wxStaticText(replacement, wxID_ANY, "Status 12:34");
        registry.synchronize();
        dynamic->SetLabel("Status 12:35");
        registry.refresh_presentation();
        require(dynamic->GetLabel() == "Status 12:35", "unrecorded label remains component-owned");

        auto *sensitive = new wxPanel(replacement);
        auto *nested = new wxPanel(sensitive);
        registry.register_sensitive(sensitive);
        require(!registry.capture_permitted(frame), "sensitive descendant blocks whole frame capture");
        require(!registry.history_permitted(replacement), "sensitive descendant blocks parent history");
        require(!registry.export_permitted(nested), "sensitive ancestor blocks child export");
        require(registry.history_identity(nested).empty(), "sensitive identity unavailable to producers");
        require(registry.capture_protection(sensitive) != SurfaceRegistry::CaptureProtection::NotRequired,
                "native protection has an explicit result");
        delete sensitive;
        require(registry.export_permitted(replacement), "destroyed sensitive subtree no longer blocks exports");
        auto *password = new wxTextCtrl(replacement, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
        require(!registry.capture_permitted(frame), "masked field excludes before explicit registration");
        require(!registry.history_permitted(password), "masked field history denied");
        registry.record_label(password, "Must not become an input value");
        require(password->GetValue().empty(), "presentation does not mutate password value");
        const auto original_password_label = password->GetLabel();
        registry.record_name(password, "Password confirmation");
        const auto accessible_name = password->GetName();
        require(!accessible_name.empty(), "sensitive field gets an explicit accessible name");
        registry.register_sensitive(password);
        password->SetName("Temporary native name");
        registry.refresh_presentation();
        require(password->GetName() == accessible_name, "neutral accessible source survives sensitivity registration");
        require(password->GetValue().empty(), "accessible-name refresh leaves password value unchanged");
        require(password->GetLabel() == original_password_label, "accessible-name refresh leaves password label unchanged");
        require(!registry.capture_permitted(password), "accessible name never removes sensitivity");
        delete password;
        auto *other = new wxFrame(nullptr, wxID_ANY, "Unknown root");
        replacement->Reparent(other);
        // An explicitly registered surface remains registered when reparented.
        require(registry.export_permitted(replacement), "explicit roots retain registration");
        require(!registry.export_permitted(other), "unknown enclosing root denied");
        delete other;
    } catch (const std::exception &error) {
        fprintf(stderr, "%s\n", error.what());
        result = 1;
    }
    delete frame;
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
