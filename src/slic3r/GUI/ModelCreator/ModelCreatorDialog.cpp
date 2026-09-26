#include "ModelCreatorDialog.hpp"

#include "../I18N.hpp"
#include "../Widgets/MaterialIcon.hpp"

#ifndef _L
#define _L(s) Slic3r::GUI::I18N::translate((s))
#endif

#include <wx/button.h>
#include <wx/app.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/utils.h>

namespace Slic3r::GUI::ModelCreator {
namespace {
wxTextCtrl *field(wxWindow *parent, wxSizer *sizer, const wxString &label,
                  long style = 0, const wxString &hint = {})
{
    auto *caption = new wxStaticText(parent, wxID_ANY, label);
    auto *control = new wxTextCtrl(parent, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, style);
    control->SetHint(hint);
    sizer->Add(caption, 0, wxTOP | wxBOTTOM, 4);
    sizer->Add(control, 0, wxEXPAND | wxBOTTOM, 8);
    return control;
}

std::filesystem::path local_workspace()
{
    return std::filesystem::path(wxStandardPaths::Get().GetUserLocalDataDir().ToStdWstring()) / "model-creator";
}
}

ModelCreatorDialog::ModelCreatorDialog(wxWindow *parent, AddToPlate add_to_plate)
    : MD3Dialog(parent, _L("Model Creator"), _L("Describe, review, then add a printable model"),
                MaterialIcon::Glyph::AutoAwesomeMosaic, MD3Dialog::Options{true, false})
    , m_add_to_plate(std::move(add_to_plate))
    , m_alive(std::make_shared<std::atomic_bool>(true))
{
    auto *body = GetContentSizer();
    body->Add(new wxStaticText(this, wxID_ANY, _L("Provider")), 0, wxBOTTOM, 4);
    m_provider = new wxChoice(this, wxID_ANY);
    m_provider->Append(_L("Claude Code CLI"));
    m_provider->Append(_L("Codex CLI"));
    m_provider->Append(_L("Anthropic API"));
    m_provider->Append(_L("OpenAI API"));
    m_provider->SetSelection(0);
    body->Add(m_provider, 0, wxEXPAND | wxBOTTOM, 8);
    m_model = field(this, body, _L("Provider model"), 0, _L("Enter an exact model ID"));
    m_provider_path = field(this, body, _L("Provider executable path"), 0, _L("Required for a CLI provider"));
    m_key = field(this, body, _L("API key"), wxTE_PASSWORD, _L("Stored in Windows Credential Manager"));
    auto *credentials = new wxBoxSizer(wxHORIZONTAL);
    auto *save = new wxButton(this, wxID_ANY, _L("Save key"));
    auto *clear = new wxButton(this, wxID_ANY, _L("Clear key"));
    credentials->Add(save, 0, wxRIGHT, 8);
    credentials->Add(clear);
    body->Add(credentials, 0, wxBOTTOM, 8);
    body->Add(new wxStaticText(this, wxID_ANY, _L("Trusted renderer")), 0, wxBOTTOM, 4);
    m_renderer = new wxChoice(this, wxID_ANY);
    m_renderer->Append(_L("OpenSCAD"));
    m_renderer->Append(_L("Blender"));
    m_renderer->SetSelection(0);
    body->Add(m_renderer, 0, wxEXPAND | wxBOTTOM, 8);
    m_renderer_path = field(this, body, _L("Renderer executable path"));
    m_prompt = field(this, body, _L("Describe the model"), wxTE_MULTILINE, _L("Dimensions are in millimeters"));
    m_prompt->SetMinSize(wxSize(-1, 90));
    m_note = field(this, body, _L("Refinement note"), wxTE_MULTILINE,
                   _L("Optional change for the next revision"));
    m_note->SetMinSize(wxSize(-1, 55));
    body->Add(new wxStaticText(this, wxID_ANY, _L("Revisions")), 0, wxBOTTOM, 4);
    m_history = new wxChoice(this, wxID_ANY);
    body->Add(m_history, 0, wxEXPAND | wxBOTTOM, 8);
    m_status = new wxTextCtrl(this, wxID_ANY, _L("No model generated"), wxDefaultPosition,
                              wxDefaultSize, wxTE_READONLY | wxBORDER_NONE);
    body->Add(m_status, 0, wxEXPAND);

    m_generate = new wxButton(this, wxID_ANY, _L("Generate"));
    m_cancel_button = new wxButton(this, wxID_ANY, _L("Cancel generation"));
    m_preview = new wxButton(this, wxID_ANY, _L("Preview mesh"));
    m_add = new wxButton(this, wxID_ANY, _L("Add to plate"));
    auto *footer = GetFooterSizer();
    footer->Add(m_generate, 0, wxRIGHT, 6);
    footer->Add(m_cancel_button, 0, wxRIGHT, 6);
    footer->Add(m_preview, 0, wxRIGHT, 6);
    footer->Add(m_add);
    m_provider->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) { update_controls(); });
    m_history->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) { select_revision(); });
    save->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { save_key(); });
    clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { clear_key(); });
    m_generate->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { generate(); });
    m_cancel_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { cancel_generation(); });
    m_preview->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        const int index = m_history->GetSelection();
        if (index != wxNOT_FOUND && index < static_cast<int>(m_revisions.size()))
            wxLaunchDefaultApplication(wxString(m_revisions[index].mesh.wstring()));
    });
    m_add->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        const int index = m_history->GetSelection();
        if (index != wxNOT_FOUND && index < static_cast<int>(m_revisions.size()) && m_add_to_plate)
            m_add_to_plate(m_revisions[index].mesh);
    });
    SetMinSize(wxSize(650, 720));
    SetSize(wxSize(720, 780));
    try {
        m_revisions = load_revisions(local_workspace());
        for (size_t i = 0; i < m_revisions.size(); ++i)
            m_history->Append(wxString::Format(_L("Revision %zu: "), i + 1) +
                              wxString::FromUTF8(m_revisions[i].scene.title));
        if (!m_revisions.empty()) {
            m_history->SetSelection(static_cast<int>(m_revisions.size() - 1));
            m_status->SetValue(_L("A retained revision is ready for preview."));
        }
    } catch (const std::exception &) {
        m_status->SetValue(_L("Saved revisions could not be read."));
    }
    update_controls();
}

ModelCreatorDialog::~ModelCreatorDialog()
{
    m_alive->store(false);
    cancel_generation();
    if (m_worker.joinable()) m_worker.join();
}

void ModelCreatorDialog::update_controls()
{
    const bool api = m_provider->GetSelection() >= 2;
    m_provider_path->Enable(!api && !m_busy);
    m_key->Enable(api && !m_busy);
    m_generate->Enable(!m_busy);
    m_cancel_button->Enable(m_busy);
    const bool selected = m_history->GetSelection() != wxNOT_FOUND;
    m_preview->Enable(selected && !m_busy);
    m_add->Enable(selected && !m_busy);
}

void ModelCreatorDialog::save_key()
{
    const Provider provider = static_cast<Provider>(m_provider->GetSelection());
    const bool saved = save_api_key(provider, m_key->GetValue().ToStdString());
    m_key->Clear();
    m_status->SetValue(saved ? _L("Key saved in the local credential vault") :
                               _L("Choose an API provider and enter a key"));
}

void ModelCreatorDialog::clear_key()
{
    const bool cleared = clear_api_key(static_cast<Provider>(m_provider->GetSelection()));
    m_key->Clear();
    m_status->SetValue(cleared ? _L("Saved key removed") : _L("No saved key was removed"));
}

void ModelCreatorDialog::generate()
{
    if (m_busy) return;
    if (m_worker.joinable()) m_worker.join();
    Settings settings;
    settings.provider = static_cast<Provider>(m_provider->GetSelection());
    settings.renderer = static_cast<Renderer>(m_renderer->GetSelection());
    settings.model = m_model->GetValue().ToStdString();
    settings.provider_executable = m_provider_path->GetValue().ToStdWstring();
    settings.renderer_executable = m_renderer_path->GetValue().ToStdWstring();
    settings.workspace = local_workspace();
    const std::string prompt = m_prompt->GetValue().ToStdString();
    const std::string note = m_note->GetValue().ToStdString();
    m_cancel = std::make_shared<std::atomic_bool>(false);
    m_busy = true;
    m_status->SetValue(_L("Generating in an isolated revision. Nothing is on the plate yet."));
    update_controls();
    m_worker = std::thread([this, settings, prompt, note, cancel = m_cancel, alive = m_alive] {
        Result result;
        try { result = run(settings, prompt, note, *cancel); }
        catch (const std::exception &) { result.error = "Model generation could not complete"; }
        if (!alive->load()) return;
        wxTheApp->CallAfter([this, alive, result = std::move(result)]() mutable {
            if (!alive->load()) return;
            m_busy = false;
            if (result) {
                m_revisions.push_back(std::move(result.revision));
                m_history->Append(wxString::Format(_L("Revision %zu: "), m_revisions.size()) +
                                  wxString::FromUTF8(m_revisions.back().scene.title));
                m_history->SetSelection(static_cast<int>(m_revisions.size() - 1));
                m_status->SetValue(_L("Mesh ready for preview. Add to plate when satisfied."));
            } else m_status->SetValue(wxString::FromUTF8(result.error));
            update_controls();
        });
    });
}

void ModelCreatorDialog::cancel_generation()
{
    if (m_cancel) m_cancel->store(true);
}

void ModelCreatorDialog::select_revision()
{
    const int index = m_history->GetSelection();
    if (index != wxNOT_FOUND && index < static_cast<int>(m_revisions.size())) {
        m_prompt->SetValue(wxString::FromUTF8(m_revisions[index].prompt));
        m_status->SetValue(_L("Selected retained revision. Add to plate when satisfied."));
    }
    update_controls();
}

} // namespace Slic3r::GUI::ModelCreator
