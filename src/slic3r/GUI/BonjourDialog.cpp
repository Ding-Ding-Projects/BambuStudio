#include "slic3r/Utils/Bonjour.hpp"   // On Windows, boost needs to be included before wxWidgets headers

#include "BonjourDialog.hpp"

#include <set>
#include <mutex>

#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/timer.h>
#include <wx/wupdlock.h>

#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/Utils/Bonjour.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3DataView.hpp"
#include "Widgets/MD3DialogChrome.hpp"
#include "Widgets/StateColor.hpp"
#include "wxExtensions.hpp"

namespace Slic3r {


class BonjourReplyEvent : public wxEvent
{
public:
	BonjourReply reply;

	BonjourReplyEvent(wxEventType eventType, int winid, BonjourReply &&reply) :
		wxEvent(winid, eventType),
		reply(std::move(reply))
	{}

	virtual wxEvent *Clone() const
	{
		return new BonjourReplyEvent(*this);
	}
};

wxDEFINE_EVENT(EVT_BONJOUR_REPLY, BonjourReplyEvent);

wxDECLARE_EVENT(EVT_BONJOUR_COMPLETE, wxCommandEvent);
wxDEFINE_EVENT(EVT_BONJOUR_COMPLETE, wxCommandEvent);

class ReplySet: public std::set<BonjourReply> {};

struct LifetimeGuard
{
	std::mutex mutex;
	BonjourDialog *dialog;

	LifetimeGuard(BonjourDialog *dialog) : dialog(dialog) {}
};

BonjourDialog::BonjourDialog(wxWindow *parent, Slic3r::PrinterTechnology tech)
	: wxDialog(parent, wxID_ANY, _(L("Network lookup")), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)
	, list(new MD3DataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_SINGLE | wxBORDER_NONE))
	, replies(new ReplySet)
	, label(new Label(this, ""))
	, timer(new wxTimer())
	, timer_state(0)
	, tech(tech)
{
	// Dialog body surface. Resolved through the kit token rather than a raw
	// white so it also reads as a surface in dark mode -- the hard *wxWHITE
	// left this dialog a bright plate under the MD3 caption strip.
	SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));

	const int em = GUI::wxGetApp().em_unit();
	// A data view asks for almost no height of its own, so the minimum keeps
	// room for the header and the rows.
	list->SetMinSize(wxSize(80 * em, 30 * em));

	// Progress line ("Searching for devices ...") in the kit body face/tone
	// instead of the OS default dialog font.
	label->SetFont(Label::Body_14);
	label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));

	wxBoxSizer *vsizer = new wxBoxSizer(wxVERTICAL);

	vsizer->Add(label, 0, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, em);

	// The results table is the kit table. A column fits its content (as the
	// native list's columns did) and never drops below its minimum width; the
	// person can still drag a column edge. Rows are inserted at the top, so the
	// table stays in descending order.
	auto add_column = [this](const wxString &title, int min_width) {
		wxDataViewColumn *column = list->AppendTextColumn(title, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
		column->SetMinWidth(min_width);
	};
	add_column(_(L("Address")), 10 * em);
	add_column(_(L("Hostname")), 10 * em);
	add_column(_(L("Service name")), 20 * em);
	if (tech == ptFFF) {
		add_column(_(L("OctoPrint version")), 10 * em);
	}
	// Styled once its columns exist, as the other kit tables are: the header
	// follows the theme, and the rows take the Material table look.
	GUI::wxGetApp().UpdateDVCDarkUI(list);
	md3_style_data_view(list);

	vsizer->Add(list, 1, wxEXPAND | wxALL, em);


    auto button_sizer = new wxBoxSizer(wxHORIZONTAL);

    // Footer: kit filled OK pill + outlined Cancel pill. The variants own their
    // background/border/text StateColors, pill radius, height, padding and label
    // font, so the hand-picked legacy greens (and the white/grey pair that stood
    // in for an outlined button) are gone with them. Kept on wxEVT_LEFT_DOWN:
    // Button::keyDownUp() re-posts Space/Enter as a synthetic left-down, so the
    // keyboard path through these handlers is unchanged.
    auto m_button_ok = new Button(this, _L("OK"));
    m_button_ok->SetMinSize(wxSize(FromDIP(96), -1));
    m_button_ok->SetVariant(Button::Variant::Filled);
    m_button_ok->SetButtonSize(Button::Size::Medium);

    m_button_ok->Bind(wxEVT_LEFT_DOWN, [this](auto &e) { this->EndModal(wxID_OK); });

    auto m_button_cancel = new Button(this, _L("Cancel"));
    m_button_cancel->SetMinSize(wxSize(FromDIP(96), -1));
    m_button_cancel->SetVariant(Button::Variant::Outlined);
    m_button_cancel->SetButtonSize(Button::Size::Medium);

    m_button_cancel->Bind(wxEVT_LEFT_DOWN, [this](auto &e) { this->EndModal(wxID_CANCEL); });

    button_sizer->AddStretchSpacer();
    button_sizer->Add(m_button_ok, 0, wxALL, FromDIP(5));
    button_sizer->Add(m_button_cancel, 0, wxALL, FromDIP(5));

	vsizer->Add(button_sizer, 0, wxALIGN_CENTER);
	SetSizerAndFit(vsizer);

	Bind(EVT_BONJOUR_REPLY, &BonjourDialog::on_reply, this);

	Bind(EVT_BONJOUR_COMPLETE, [this](wxCommandEvent &) {
		this->timer_state = 0;
	});

	Bind(wxEVT_TIMER, &BonjourDialog::on_timer, this);
	GUI::wxGetApp().UpdateDlgDarkUI(this);
	MD3DialogCaption::Adopt(this);
}

BonjourDialog::~BonjourDialog()
{
	// Needed bacuse of forward defs
}

bool BonjourDialog::show_and_lookup()
{
	Show();   // Because we need GetId() to work before ShowModal()

	timer->Stop();
	timer->SetOwner(this);
	timer_state = 1;
	timer->Start(1000);
    on_timer_process();

	// The background thread needs to queue messages for this dialog
	// and for that it needs a valid pointer to it (mandated by the wxWidgets API).
	// Here we put the pointer under a shared_ptr and protect it by a mutex,
	// so that both threads can access it safely.
	auto dguard = std::make_shared<LifetimeGuard>(this);

	// Note: More can be done here when we support discovery of hosts other than Octoprint and SL1
	Bonjour::TxtKeys txt_keys { "version", "model" };

    bonjour = Bonjour("octoprint")
		.set_txt_keys(std::move(txt_keys))
		.set_retries(3)
		.set_timeout(4)
		.on_reply([dguard](BonjourReply &&reply) {
			std::lock_guard<std::mutex> lock_guard(dguard->mutex);
			auto dialog = dguard->dialog;
			if (dialog != nullptr) {
				auto evt = new BonjourReplyEvent(EVT_BONJOUR_REPLY, dialog->GetId(), std::move(reply));
				wxQueueEvent(dialog, evt);
			}
		})
		.on_complete([dguard]() {
			std::lock_guard<std::mutex> lock_guard(dguard->mutex);
			auto dialog = dguard->dialog;
			if (dialog != nullptr) {
				auto evt = new wxCommandEvent(EVT_BONJOUR_COMPLETE, dialog->GetId());
				wxQueueEvent(dialog, evt);
			}
		})
		.lookup();

	bool res = ShowModal() == wxID_OK && list->GetSelectedRow() != wxNOT_FOUND;
	{
		// Tell the background thread the dialog is going away...
		std::lock_guard<std::mutex> lock_guard(dguard->mutex);
		dguard->dialog = nullptr;
	}
	return res;
}

wxString BonjourDialog::get_selected() const
{
	// The first column is the address.
	const int row = list->GetSelectedRow();
	return row != wxNOT_FOUND ? list->GetTextValue(row, 0) : wxString();
}


// Private

void BonjourDialog::on_reply(BonjourReplyEvent &e)
{
	if (replies->find(e.reply) != replies->end()) {
		// We already have this reply
		return;
	}

	// Filter replies based on selected technology
	const auto model = e.reply.txt_data.find("model");
	const bool sl1 = model != e.reply.txt_data.end() && model->second == "SL1";
	if ((tech == ptFFF && sl1) || (tech == ptSLA && !sl1)) {
		return;
	}

	replies->insert(std::move(e.reply));

	auto selected = get_selected();

	wxWindowUpdateLocker freeze_guard(this);
	(void)freeze_guard;

	list->DeleteAllItems();

	// The whole table is recreated so that we benefit from it already being sorted in the set.
	// Every row carries one value per column: the version cell exists only for FFF,
	// where the OctoPrint version column does, and is empty when the reply has no version.
	for (const auto &reply : *replies) {
		wxVector<wxVariant> row;
		row.push_back(wxVariant(GUI::from_u8(reply.full_address)));
		row.push_back(wxVariant(GUI::from_u8(reply.hostname)));
		row.push_back(wxVariant(GUI::from_u8(reply.service_name)));

		if (tech == ptFFF) {
			const auto it = reply.txt_data.find("version");
			row.push_back(wxVariant(it != reply.txt_data.end() ? GUI::from_u8(it->second) : wxString()));
		}
		list->InsertItem(0, row);
	}

	if (!selected.IsEmpty()) {
		// Attempt to preserve selection
		for (int r = 0; r < list->GetItemCount(); ++r) {
			if (list->GetTextValue(r, 0) == selected) {
				list->SelectRow(r);
				break;
			}
		}
	}
}

void BonjourDialog::on_timer(wxTimerEvent &)
{
    on_timer_process();
}

// This is here so the function can be bound to wxEVT_TIMER and also called
// explicitly (wxTimerEvent should not be created by user code).
void BonjourDialog::on_timer_process()
{
    const auto search_str = _utf8(L("Searching for devices"));

    if (timer_state > 0) {
        const std::string dots(timer_state, '.');
        label->SetLabel(GUI::from_u8((boost::format("%1% %2%") % search_str % dots).str()));
        timer_state = (timer_state) % 3 + 1;
    } else {
        label->SetLabel(GUI::from_u8((boost::format("%1%: %2%") % search_str % (_utf8(L("Finished"))+".")).str()));
        timer->Stop();
    }
}




}
