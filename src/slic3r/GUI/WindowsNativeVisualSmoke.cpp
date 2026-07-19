#include "WindowsNativeVisualSmoke.hpp"

#include "LanguageMode.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

#include <wx/app.h>
#include <wx/colour.h>
#include <wx/font.h>
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/utils.h>

namespace Slic3r::GUI {
namespace {

constexpr char        SMOKE_ENVIRONMENT[]   = "BAMBU_STUDIO_CI_NATIVE_VISUAL_SMOKE";
constexpr char        SMOKE_PROTOCOL[]      = "v1/";
constexpr std::size_t SMOKE_PROTOCOL_LENGTH = sizeof(SMOKE_PROTOCOL) - 1;

struct VisualScenario
{
    const char *id;
    const char *language_mode;
    const char *theme;
    const char *window_suffix;
    const char *headline;
    const char *supporting_text;
    const char *primary_action;
    bool        dark;
    wxColour    accent;
};

const std::array<VisualScenario, 3> VISUAL_SCENARIOS{
    {{"light-en", I18N::LANGUAGE_MODE_ENGLISH, "light", "English", "Prepare, Slice, Print", "English interface mode", "Start a new project", false, wxColour(0, 109, 59)},
     {"dark-yue_HK", I18N::LANGUAGE_MODE_CANTONESE_HONG_KONG, "dark", u8"廣東話（香港）", u8"準備、切片、列印", u8"廣東話（香港）介面模式", u8"開始新專案", true,
      wxColour(80, 222, 137)},
     {"light-bilingual", I18N::LANGUAGE_MODE_ENGLISH_CANTONESE_HK, "light", u8"English + 廣東話（香港）", u8"Prepare, Slice, Print · 準備、切片、列印",
      u8"English + 廣東話（香港） compact bilingual mode", u8"New project · 新專案", false, wxColour(94, 53, 177)}}};

bool environment_equals(const wxString &name, const wxString &expected)
{
    wxString value;
    return wxGetEnv(name, &value) && value == expected;
}

wxFont visual_font(int point_size, bool bold = false)
{
    wxFont font(wxFontInfo(point_size).Family(wxFONTFAMILY_SWISS).FaceName("Microsoft JhengHei UI"));
    if (bold) font.MakeBold();
    return font;
}

wxStaticText *add_text(wxWindow       *parent,
                       wxSizer        *sizer,
                       const wxString &text,
                       const wxColour &foreground,
                       const wxColour &background,
                       int             point_size,
                       bool            bold       = false,
                       int             proportion = 0,
                       int             flags      = wxEXPAND,
                       int             border     = 0)
{
    auto *label = new wxStaticText(parent, wxID_ANY, text);
    label->SetName(text);
    label->SetForegroundColour(foreground);
    label->SetBackgroundColour(background);
    label->SetFont(visual_font(point_size, bold));
    sizer->Add(label, proportion, flags, border);
    return label;
}

wxPanel *make_metric_card(wxWindow *parent, const wxString &caption, const wxString &value, const wxColour &surface, const wxColour &primary_text, const wxColour &secondary_text)
{
    auto *card = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    card->SetBackgroundColour(surface);

    auto *card_sizer = new wxBoxSizer(wxVERTICAL);
    add_text(card, card_sizer, caption, secondary_text, surface, 9, true);
    card_sizer->AddSpacer(10);
    add_text(card, card_sizer, value, primary_text, surface, 13, true);
    card->SetSizer(card_sizer);
    card_sizer->Fit(card);
    return card;
}

wxFrame *create_visual_window(const VisualScenario &scenario)
{
    const wxColour background = scenario.dark ? wxColour(27, 29, 28) : wxColour(246, 248, 246);
    const wxColour surface    = scenario.dark ? wxColour(39, 43, 41) : wxColour(255, 255, 255);
    const wxColour surface_2  = scenario.dark ? wxColour(48, 52, 50) : wxColour(232, 240, 233);
    const wxColour text       = scenario.dark ? wxColour(233, 235, 232) : wxColour(27, 29, 28);
    const wxColour secondary  = scenario.dark ? wxColour(191, 197, 191) : wxColour(74, 81, 75);
    const wxColour on_accent  = scenario.dark ? wxColour(0, 57, 30) : wxColour(255, 255, 255);

    const wxString title = wxString::FromUTF8("Bambu Studio MD3 Native Visual Smoke [") + wxString::FromUTF8(scenario.id) + wxString::FromUTF8("] | ") +
                           wxString::FromUTF8(scenario.window_suffix);

    auto *frame = new wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(920, 600), (wxDEFAULT_FRAME_STYLE & ~(wxRESIZE_BORDER | wxMAXIMIZE_BOX)));
    frame->SetName("BambuStudioNativeVisualSmokeV1");
    frame->SetClientSize(wxSize(920, 600));
    frame->SetBackgroundColour(background);

    auto *root = new wxPanel(frame, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    root->SetName("BambuStudioNativeVisualSmokeRoot");
    root->SetBackgroundColour(background);

    auto *layout = new wxBoxSizer(wxVERTICAL);

    auto *accent_bar = new wxPanel(root, wxID_ANY, wxDefaultPosition, wxSize(-1, 10), wxBORDER_NONE);
    accent_bar->SetName("MaterialPrimaryAccent");
    accent_bar->SetBackgroundColour(scenario.accent);
    accent_bar->SetMinSize(wxSize(-1, 10));
    layout->Add(accent_bar, 0, wxEXPAND);

    auto *content = new wxBoxSizer(wxVERTICAL);
    content->AddSpacer(30);
    add_text(root, content, "Bambu Studio MD3", text, background, 14, true);
    content->AddSpacer(8);
    add_text(root, content, wxString::FromUTF8(scenario.headline), text, background, 27, true);
    content->AddSpacer(8);
    add_text(root, content, wxString::FromUTF8(scenario.supporting_text), secondary, background, 13);
    content->AddSpacer(26);

    auto                                              *metrics = new wxBoxSizer(wxHORIZONTAL);
    const std::array<std::pair<wxString, wxString>, 3> metric_values{
        {{"LANGUAGE MODE", wxString::FromUTF8(scenario.language_mode)}, {"MATERIAL THEME", wxString::FromUTF8(scenario.theme)}, {"RENDER PATH", "Native wxWidgets"}}};
    for (std::size_t index = 0; index < metric_values.size(); ++index) {
        auto *card = make_metric_card(root, metric_values[index].first, metric_values[index].second, surface, text, secondary);
        metrics->Add(card, 1, wxEXPAND | (index == 0 ? 0 : wxLEFT), index == 0 ? 0 : 14);
    }
    content->Add(metrics, 0, wxEXPAND);
    content->AddSpacer(22);

    auto *action_surface = new wxPanel(root, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    action_surface->SetName("MaterialPrimaryAction");
    action_surface->SetBackgroundColour(scenario.accent);
    auto *action_sizer = new wxBoxSizer(wxVERTICAL);
    action_sizer->AddSpacer(15);
    add_text(action_surface, action_sizer, wxString::FromUTF8(scenario.primary_action), on_accent, scenario.accent, 13, true, 0, wxALIGN_CENTER_HORIZONTAL);
    action_sizer->AddSpacer(15);
    action_surface->SetSizer(action_sizer);
    content->Add(action_surface, 0, wxEXPAND);
    content->AddSpacer(22);

    auto *evidence_surface = new wxPanel(root, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    evidence_surface->SetName("NativeVisualEvidenceIdentity");
    evidence_surface->SetBackgroundColour(surface_2);
    auto *evidence_sizer = new wxBoxSizer(wxVERTICAL);
    evidence_sizer->AddSpacer(12);
    add_text(evidence_surface, evidence_sizer, "BAMBU_STUDIO_NATIVE_VISUAL_SMOKE_V1", text, surface_2, 9, true, 0, wxLEFT | wxRIGHT, 16);
    add_text(evidence_surface, evidence_sizer, wxString::FromUTF8("SCENARIO=") + wxString::FromUTF8(scenario.id), secondary, surface_2, 9, false, 0, wxLEFT | wxRIGHT, 16);
    add_text(evidence_surface, evidence_sizer, wxString::FromUTF8("LANGUAGE_MODE=") + wxString::FromUTF8(scenario.language_mode), secondary, surface_2, 9, false, 0,
             wxLEFT | wxRIGHT, 16);
    add_text(evidence_surface, evidence_sizer, wxString::FromUTF8("THEME=") + wxString::FromUTF8(scenario.theme), secondary, surface_2, 9, false, 0, wxLEFT | wxRIGHT, 16);
    evidence_sizer->AddSpacer(12);
    evidence_surface->SetSizer(evidence_sizer);
    content->Add(evidence_surface, 0, wxEXPAND);
    content->AddStretchSpacer(1);
    add_text(root, content, wxString::FromUTF8(u8"Windows-first deterministic native evidence · no wizard · no network"), secondary, background, 9);
    content->AddSpacer(22);

    layout->Add(content, 1, wxEXPAND | wxLEFT | wxRIGHT, 34);
    root->SetSizer(layout);
    auto *frame_layout = new wxBoxSizer(wxVERTICAL);
    frame_layout->Add(root, 1, wxEXPAND);
    frame->SetSizer(frame_layout);
    frame->Layout();
    frame->CenterOnScreen();
    return frame;
}

} // namespace

WindowsNativeVisualSmokeResult try_start_windows_native_visual_smoke()
{
    wxString request;
    if (!wxGetEnv(SMOKE_ENVIRONMENT, &request)) return WindowsNativeVisualSmokeResult::NotRequested;

    // The benign smoke surface is still double-guarded so it cannot be reached
    // accidentally during an ordinary desktop launch.
    if (!environment_equals("CI", "true") || !environment_equals("GITHUB_ACTIONS", "true") || !environment_equals("RUNNER_ENVIRONMENT", "github-hosted") ||
        !request.StartsWith(SMOKE_PROTOCOL))
        return WindowsNativeVisualSmokeResult::Rejected;

    const wxString requested_id = request.Mid(SMOKE_PROTOCOL_LENGTH);
    const auto     scenario     = std::find_if(VISUAL_SCENARIOS.begin(), VISUAL_SCENARIOS.end(),
                                               [&requested_id](const VisualScenario &candidate) { return requested_id == wxString::FromUTF8(candidate.id); });
    if (scenario == VISUAL_SCENARIOS.end()) return WindowsNativeVisualSmokeResult::Rejected;

    wxFrame *frame = create_visual_window(*scenario);
    wxTheApp->SetTopWindow(frame);
    wxTheApp->SetExitOnFrameDelete(true);
    frame->Show(true);
    frame->Raise();
    return WindowsNativeVisualSmokeResult::Started;
}

} // namespace Slic3r::GUI
