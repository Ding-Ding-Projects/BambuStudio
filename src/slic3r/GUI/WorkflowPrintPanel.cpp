#include "WorkflowPrintPanel.hpp"
#include "I18N.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/StateColor.hpp"
#include <wx/stattext.h>
#include <wx/wrapsizer.h>
#include <algorithm>

namespace Slic3r { namespace GUI {

WorkflowPrintPanel::WorkflowPrintPanel(wxWindow* parent, ReadSummary read_summary, RunAction run_action)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxTAB_TRAVERSAL),
      m_read_summary(std::move(read_summary)), m_run_action(std::move(run_action))
{
    SetName(_L("Print preparation"));
    SetScrollRate(0, FromDIP(16));
    auto* root = new wxBoxSizer(wxVERTICAL);
    const int padding = FromDIP(MD3::Metrics::active().padding);
    const int gap = FromDIP(MD3::Metrics::active().gap);
    AddText(this, root, _L("Review before printing"), true);
    AddText(this, root, _L("Review the selected plate and choose an output action. Opening this page does not slice or send a job."));

    m_columns = new wxBoxSizer(wxHORIZONTAL);
    auto* steps = new wxBoxSizer(wxVERTICAL);
    m_columns->Add(steps, 2, wxEXPAND);
    m_spacing.emplace_back(root->Add(m_columns, 0, wxEXPAND | wxALL, padding), false);
    auto card = [this, gap](wxSizer* owner, const wxString& title) {
        auto* box = new StaticBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        auto* content = new wxBoxSizer(wxVERTICAL);
        box->SetSizer(content);
        AddText(box, content, title, true);
        if (owner) m_spacing.emplace_back(owner->Add(box, 0, wxEXPAND | wxBOTTOM, gap), true);
        m_spacers.push_back(content->AddSpacer(gap));
        m_cards.push_back(box);
        return box;
    };

    auto* readiness = card(steps, _L("Plate readiness"));
    m_status = AddText(readiness, readiness->GetSizer(), wxEmptyString);
    m_reason = AddText(readiness, readiness->GetSizer(), wxEmptyString);
    auto* slice_actions = new wxWrapSizer(wxHORIZONTAL);
    m_slice = AddAction(readiness, slice_actions, _L("Slice plate"), WorkflowPrint::Action::Slice);
    m_slice_print = AddAction(readiness, slice_actions, _L("Slice and print"), WorkflowPrint::Action::SliceAndPrint);
    m_slice_send = AddAction(readiness, slice_actions, _L("Slice and send"), WorkflowPrint::Action::SliceAndSend);
    m_spacing.emplace_back(readiness->GetSizer()->Add(slice_actions, 0, wxEXPAND | wxALL, padding), false);

    auto* destination = card(steps, _L("Destination and mapping"));
    AddText(destination, destination->GetSizer(), _L("Printer destination and material mapping are reviewed in the selected action's existing confirmation flow."));
    auto* options = new wxWrapSizer(wxHORIZONTAL);
    AddAction(destination, options, _L("Output options"), WorkflowPrint::Action::OutputOptions);
    m_spacing.emplace_back(destination->GetSizer()->Add(options, 0, wxEXPAND | wxALL, padding), false);

    auto* continuation = card(steps, _L("Continue deliberately"));
    AddText(continuation, continuation->GetSizer(), _L("The selected action keeps its existing printer selection, validation, and confirmation steps."));
    AddText(continuation, continuation->GetSizer(), _L("Slicing and combined output actions use the same checks and confirmation flow as Prepare."));
    auto* navigation = new wxWrapSizer(wxHORIZONTAL);
    AddAction(continuation, navigation, _L("Prepare"), WorkflowPrint::Action::Prepare);
    AddAction(continuation, navigation, _L("Preview"), WorkflowPrint::Action::Preview);
    AddAction(continuation, navigation, _L("Monitor"), WorkflowPrint::Action::Monitor);
    m_spacing.emplace_back(continuation->GetSizer()->Add(navigation, 0, wxEXPAND | wxALL, padding), false);

    auto* summary = card(nullptr, _L("Print summary"));
    m_summary_item = m_columns->Add(summary, 1, wxEXPAND | wxLEFT, gap);
    m_plate = AddText(summary, summary->GetSizer(), wxEmptyString);
    m_printer = AddText(summary, summary->GetSizer(), wxEmptyString);
    m_estimate = AddText(summary, summary->GetSizer(), wxEmptyString);
    m_material = AddText(summary, summary->GetSizer(), wxEmptyString);
    AddText(summary, summary->GetSizer(), _L("Estimates describe the selected plate. The output action determines whether one or all plates are included."));
    auto* output = new wxWrapSizer(wxHORIZONTAL);
    m_output = AddAction(summary, output, _L("Print"), WorkflowPrint::Action::Output, true);
    m_spacing.emplace_back(summary->GetSizer()->Add(output, 0, wxEXPAND | wxALL, padding), false);
    SetSizer(root);
    ApplyTheme();
    Bind(wxEVT_SIZE, [this](wxSizeEvent& event) { Reflow(); event.Skip(); });
    Bind(wxEVT_SHOW, [this](wxShowEvent& event) { if (event.IsShown()) RefreshSummary(); event.Skip(); });
}

wxStaticText* WorkflowPrintPanel::AddText(wxWindow* parent, wxSizer* sizer, const wxString& text, bool heading)
{
    auto* label = new wxStaticText(parent, wxID_ANY, text, wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
    label->SetMinSize(wxSize(1, -1));
    m_spacing.emplace_back(sizer->Add(label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
        FromDIP(MD3::Metrics::active().padding)), false);
    m_text.push_back(label);
    m_labels[label] = text;
    if (heading) m_headings.push_back(label);
    return label;
}

Button* WorkflowPrintPanel::AddAction(wxWindow* parent, wxSizer* sizer, const wxString& label,
                                     WorkflowPrint::Action action, bool primary)
{
    auto* button = new Button(parent, label);
    button->SetName(label);
    button->SetVariant(primary ? Button::Variant::Filled : Button::Variant::Outlined);
    button->SetButtonSize(Button::Size::Large);
    button->SetColorScheme(MD3::ColorScheme::Device);
    button->Bind(wxEVT_BUTTON, [this, action, button](wxCommandEvent&) {
        WorkflowPrint::dispatch(action, [this]() { RefreshSummary(); return m_summary.availability; },
            [this, button](WorkflowPrint::Action selected) { m_run_action(selected, button); });
    });
    m_spacing.emplace_back(sizer->Add(button, 0, wxRIGHT | wxBOTTOM, FromDIP(MD3::Metrics::active().gap)), true);
    m_buttons.push_back(button);
    return button;
}

void WorkflowPrintPanel::RefreshSummary()
{
    if (!m_read_summary) return;
    m_summary = m_read_summary();
    SetText(m_plate, m_summary.plate);
    SetText(m_printer, m_summary.printer);
    SetText(m_estimate, m_summary.estimate);
    SetText(m_material, m_summary.material);
    switch (m_summary.availability.status()) {
    case WorkflowPrint::Status::Empty: SetText(m_status, _L("No printable content")); break;
    case WorkflowPrint::Status::Slicing: SetText(m_status, _L("Slicing in progress")); break;
    case WorkflowPrint::Status::NeedsSlice: SetText(m_status, _L("Slice required")); break;
    case WorkflowPrint::Status::Ready: SetText(m_status, _L("Ready for the selected output action")); break;
    case WorkflowPrint::Status::Unavailable: SetText(m_status, _L("Output unavailable")); break;
    }
    SetText(m_reason, m_summary.disabled_reason.empty()
        ? _L("The selected action keeps its existing printer selection, validation, and confirmation steps.")
        : m_summary.disabled_reason);
    m_output->SetLabel(m_summary.output_label);
    m_output->SetName(m_summary.output_label);
    m_output->Enable(m_summary.availability.allows(WorkflowPrint::Action::Output));
    m_output->SetToolTip(m_summary.disabled_reason);
    m_slice->SetLabel(m_summary.slice_label);
    m_slice->SetName(m_summary.slice_label);
    m_slice->Enable(m_summary.availability.allows(WorkflowPrint::Action::Slice));
    m_slice_print->Enable(m_summary.availability.allows(WorkflowPrint::Action::SliceAndPrint));
    m_slice_send->Enable(m_summary.availability.allows(WorkflowPrint::Action::SliceAndSend));
    const wxString slice_reason = _L("Return to Prepare to review the plate, material settings, or slicing progress.");
    for (auto* button : {m_slice, m_slice_print, m_slice_send})
        button->SetToolTip(button->IsEnabled() ? wxEmptyString : slice_reason);
    Reflow();
}

void WorkflowPrintPanel::SetText(wxStaticText* label, const wxString& text)
{
    m_labels[label] = text;
    label->SetLabel(text);
}

void WorkflowPrintPanel::Reflow()
{
    if (m_reflowing || GetClientSize().x <= 0) return;
    m_reflowing = true;
    const int padding = FromDIP(MD3::Metrics::active().padding);
    const int columns_min = m_columns->GetItem(size_t(0))->CalcMin().x +
                            m_summary_item->CalcMin().x + FromDIP(MD3::Metrics::active().gap);
    const bool stacked = GetClientSize().x < std::max(FromDIP(920), columns_min + 2 * padding);
    m_columns->SetOrientation(stacked ? wxVERTICAL : wxHORIZONTAL);
    m_columns->GetItem(size_t(0))->SetProportion(stacked ? 0 : 2);
    m_summary_item->SetProportion(stacked ? 0 : 1);
    m_summary_item->SetFlag(wxEXPAND | (stacked ? wxTOP : wxLEFT));
    m_summary_item->SetBorder(FromDIP(MD3::Metrics::active().gap));
    Layout();
    for (auto* label : m_text) {
        // Restore the unwrapped label before wrapping to the new width.
        label->SetLabel(m_labels.at(label));
        const int width = std::max(FromDIP(40), label->GetParent()->GetClientSize().x - 2 * padding);
        label->Wrap(width);
    }
    Layout();
    FitInside();
    m_reflowing = false;
}

void WorkflowPrintPanel::ApplyTheme()
{
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    for (const auto& spacing : m_spacing)
        spacing.first->SetBorder(FromDIP(spacing.second ? MD3::Metrics::active().gap : MD3::Metrics::active().padding));
    for (auto* spacer : m_spacers)
        spacer->SetMinSize(wxSize(0, FromDIP(MD3::Metrics::active().gap)));
    for (auto* card : m_cards) {
        card->SetDensity(MD3::Metrics::isCompact() ? StaticBox::Density::Compact : StaticBox::Density::Comfortable);
        card->SetBackgroundColor(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
        card->SetBorderColor(StateColor::semantic(MD3::Role::OutlineVariant));
        card->SyncWindowBackground();
    }
    for (auto* label : m_text) {
        label->SetFont(MD3::Metrics::isCompact() ? ::Label::Body_13 : ::Label::Body_14);
        label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        label->SetBackgroundColour(label->GetParent() == this ? GetBackgroundColour()
            : StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    }
    for (auto* heading : m_headings) {
        heading->SetFont(heading == m_headings.front() ? ::Label::Head_20 : ::Label::Head_16);
        heading->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    }
    for (auto* button : m_buttons) button->Rescale();
    Reflow();
    Refresh();
}

void WorkflowPrintPanel::Rescale()
{
    SetScrollRate(0, FromDIP(16));
    ApplyTheme();
}

}} // namespace Slic3r::GUI
