#include "SmartHomeDialog.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"
#include "TtsNarrator.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/CheckBox.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/SearchField.hpp"
#include "Widgets/StateColor.hpp"

#include "libslic3r/AppConfig.hpp"

#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/textctrl.h>

namespace Slic3r { namespace GUI {

namespace {

// Append `value` to the semicolon-separated config list `key` (deduped).
void append_to_config_list(const char *key, const std::string &value)
{
    AppConfig *cfg = wxGetApp().app_config;
    std::string raw = cfg->get(key);
    if ((";" + raw + ";").find(";" + value + ";") != std::string::npos)
        return;
    if (!raw.empty())
        raw += ";";
    raw += value;
    cfg->set(key, raw);
    cfg->save();
}

wxString config_list_pretty(const char *key, const wxString &empty_text)
{
    const std::string raw = wxGetApp().app_config->get(key);
    return raw.empty() ? empty_text : wxString::FromUTF8(raw);
}

} // namespace

SmartHomeDialog::SmartHomeDialog(wxWindow *parent)
    : MD3Dialog(parent, _L("Smart home"),
                _L("Home Assistant speakers, media controls and alert lights."),
                MaterialIcon::Cast)
{
    AppConfig *cfg = wxGetApp().app_config;
    wxBoxSizer *body = GetContentSizer();
    const wxColour bg = GetBackgroundColour();
    const wxColour on = StateColor::semantic(MD3::Role::OnSurface);
    const wxColour on_var = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    auto label = [&](const wxString &text, bool secondary = false) {
        auto *l = new Label(this, secondary ? Label::Body_12 : Label::Body_13, text);
        l->SetBackgroundColour(bg);
        l->SetForegroundColour(secondary ? on_var : on);
        return l;
    };

    // --- connection ---------------------------------------------------------
    auto *conn = new wxBoxSizer(wxHORIZONTAL);
    conn->Add(label(_L("Home Assistant URL")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    m_url = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(cfg->get("ha_url")),
                           wxDefaultPosition, wxSize(FromDIP(220), -1));
    conn->Add(m_url, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    conn->Add(label(_L("Token")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    m_token = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(cfg->get("ha_token")),
                             wxDefaultPosition, wxSize(FromDIP(160), -1), wxTE_PASSWORD);
    conn->Add(m_token, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    auto *connect = new Button(this, _L("Connect"));
    connect->SetMinSize(FromDIP(wxSize(96, 34)));
    connect->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        AppConfig *c = wxGetApp().app_config;
        c->set("ha_url", std::string(m_url->GetValue().ToUTF8()));
        c->set("ha_token", std::string(m_token->GetValue().ToUTF8()));
        c->save();
        refresh_entities();
    });
    conn->Add(connect, 0, wxALIGN_CENTER_VERTICAL);
    body->Add(conn, 0, wxEXPAND | wxALL, FromDIP(10));

    m_status = label(_L("Not connected yet. Enter the URL and a long-lived access token, then press Connect."), true);
    m_status->Wrap(FromDIP(520));
    body->Add(m_status, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(10));

    // --- entity browser: search + list --------------------------------------
    m_search = new SearchField(this, _L("Search speakers and lights"));
    m_search->SetOnQuery([this](const wxString &) { rebuild_list(); });
    m_search->SetOnRegexToggle([this](bool) { rebuild_list(); });
    body->Add(m_search, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));
    m_list = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(150)));
    body->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));

    // --- media controls ------------------------------------------------------
    auto *media = new wxBoxSizer(wxHORIZONTAL);
    struct Ctl { const wchar_t *glyph_label; const char *service; };
    const std::vector<std::pair<wxString, std::string>> controls = {
        {_L("Previous"), "media_previous_track"},
        {_L("Play / Pause"), "media_play_pause"},
        {_L("Next"), "media_next_track"},
    };
    for (const auto &[text, service] : controls) {
        auto *b = new Button(this, text);
        b->SetMinSize(FromDIP(wxSize(110, 34)));
        b->Bind(wxEVT_BUTTON, [this, service = service](wxCommandEvent &) {
            if (const auto *entity = selected_entity())
                HomeAssistant::media_command(entity->entity_id, service);
        });
        media->Add(b, 0, wxRIGHT, FromDIP(6));
    }
    media->Add(label(_L("Volume")), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(8));
    m_volume = new wxSlider(this, wxID_ANY, 50, 0, 100, wxDefaultPosition, wxSize(FromDIP(140), -1));
    m_volume->Bind(wxEVT_SLIDER, [this](wxCommandEvent &) {
        if (const auto *entity = selected_entity())
            HomeAssistant::media_volume(entity->entity_id, m_volume->GetValue() / 100.0);
    });
    media->Add(m_volume, 0, wxALIGN_CENTER_VERTICAL);
    body->Add(media, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(10));

    // --- add-as buttons ------------------------------------------------------
    auto *adds = new wxBoxSizer(wxHORIZONTAL);
    auto *add_speaker = new Button(this, _L("Use as announcement speaker"));
    add_speaker->SetMinSize(FromDIP(wxSize(210, 34)));
    add_speaker->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (const auto *entity = selected_entity();
            entity != nullptr && entity->entity_id.rfind("media_player.", 0) == 0) {
            append_to_config_list("ha_speakers", entity->entity_id);
            m_speakers_label->SetLabel(config_list_pretty("ha_speakers", _L("none")));
            Layout();
        }
    });
    adds->Add(add_speaker, 0, wxRIGHT, FromDIP(8));
    auto *add_light = new Button(this, _L("Use as alert light"));
    add_light->SetMinSize(FromDIP(wxSize(160, 34)));
    add_light->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (const auto *entity = selected_entity();
            entity != nullptr && entity->entity_id.rfind("light.", 0) == 0) {
            append_to_config_list("ha_lights", entity->entity_id);
            m_lights_label->SetLabel(config_list_pretty("ha_lights", _L("none")));
            Layout();
        }
    });
    adds->Add(add_light, 0);
    body->Add(adds, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(10));

    auto *speakers_row = new wxBoxSizer(wxHORIZONTAL);
    speakers_row->Add(label(_L("Announcement speakers:"), true), 0, wxRIGHT, FromDIP(6));
    m_speakers_label = label(config_list_pretty("ha_speakers", _L("none")), true);
    speakers_row->Add(m_speakers_label, 1);
    body->Add(speakers_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(6));
    auto *lights_row = new wxBoxSizer(wxHORIZONTAL);
    lights_row->Add(label(_L("Alert lights:"), true), 0, wxRIGHT, FromDIP(6));
    m_lights_label = label(config_list_pretty("ha_lights", _L("none")), true);
    lights_row->Add(m_lights_label, 1);
    body->Add(lights_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(2));

    // --- toggles -------------------------------------------------------------
    auto add_toggle = [&](CheckBox *&store, const char *key, const wxString &text, bool default_on) {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        store = new CheckBox(this);
        const std::string value = cfg->get(key);
        store->SetValue(value.empty() ? default_on : value == "true");
        store->Bind(wxEVT_TOGGLEBUTTON, [key, store](wxCommandEvent &e) {
            wxGetApp().app_config->set(key, store->GetValue() ? "true" : "false");
            wxGetApp().app_config->save();
            e.Skip();
        });
        row->Add(store, 0, wxALIGN_CENTER_VERTICAL);
        row->Add(label(text), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        body->Add(row, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(8));
    };
    // Narrator is OFF by default (narrator rules); light flashes default on
    // once lights are chosen, since choosing them is the explicit opt-in.
    add_toggle(m_narrator_toggle, "narrator_enabled",
               _L("Speak printer state changes and errors (TTS narrator)"), false);
    add_toggle(m_flash_error_toggle, "ha_flash_on_error",
               _L("Flash alert lights red on printer errors (auto-restores)"), true);
    add_toggle(m_flash_finish_toggle, "ha_flash_on_finish",
               _L("Pulse alert lights green when a print finishes (auto-restores)"), true);

    auto *close = new Button(this, _L("Close"), "", 0, 0, wxID_CANCEL);
    close->SetMinSize(FromDIP(wxSize(104, 40)));
    close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    GetFooterSizer()->AddStretchSpacer();
    GetFooterSizer()->Add(close, 0, wxALIGN_CENTER_VERTICAL);

    SetMinSize(FromDIP(wxSize(600, 640)));
    Layout();
    Fit();
    CenterOnParent();
    if (HomeAssistant::configured())
        refresh_entities();
}

void SmartHomeDialog::refresh_entities()
{
    m_status->SetLabel(_L("Connecting to Home Assistant..."));
    Layout();
    HomeAssistant::list_entities("", [this](std::vector<HomeAssistant::Entity> entities, std::string error) {
        if (!error.empty()) {
            m_status->SetLabel(wxString::Format(_L("Home Assistant is unreachable: %s"),
                                                wxString::FromUTF8(error)));
            Layout();
            return;
        }
        m_entities.clear();
        for (auto &entity : entities)
            if (entity.entity_id.rfind("media_player.", 0) == 0 || entity.entity_id.rfind("light.", 0) == 0)
                m_entities.push_back(std::move(entity));
        m_status->SetLabel(wxString::Format(_L("Connected. %d speakers and lights found."),
                                            (int) m_entities.size()));
        rebuild_list();
        Layout();
    });
}

void SmartHomeDialog::rebuild_list()
{
    const wxString query = m_search->GetValue();
    const bool regex      = m_search->IsRegexEnabled();
    const bool case_sense = m_search->IsCaseSensitive();
    const bool whole_word = m_search->IsWholeWord();
    const bool multiline  = m_search->IsMultiline();
    m_list->Clear();
    m_visible.clear();
    SearchField::MatchPass match_pass(query, regex, case_sense, whole_word, multiline);
    for (int i = 0; i < (int) m_entities.size(); ++i) {
        const auto &entity = m_entities[i];
        const wxString hay = wxString::FromUTF8(entity.friendly_name + " " + entity.entity_id);
        if (!query.IsEmpty() && !match_pass.matches(hay))
            continue;
        m_list->Append(wxString::Format("%s  (%s, %s)",
                                        wxString::FromUTF8(entity.friendly_name),
                                        wxString::FromUTF8(entity.entity_id),
                                        wxString::FromUTF8(entity.state)));
        m_visible.push_back(i);
    }
}

const HomeAssistant::Entity *SmartHomeDialog::selected_entity() const
{
    const int sel = m_list->GetSelection();
    if (sel == wxNOT_FOUND || sel >= (int) m_visible.size())
        return nullptr;
    return &m_entities[m_visible[sel]];
}

} } // namespace Slic3r::GUI
