#include "RackColdPullDialog.hpp"
#include "RackColdPullQueue.hpp"
#include "slic3r/GUI/DeviceCore/DevManager.h"
#include "slic3r/GUI/DeviceCore/DevNozzleRack.h"
#include "slic3r/GUI/DeviceCore/DevExtruderSystem.h"
#include "slic3r/GUI/DeviceManager.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/timer.h>
#include <chrono>

namespace Slic3r::GUI {
namespace {
using ColdPull::Step;
using Clock = std::chrono::steady_clock;

ColdPull::Reading read_machine(const std::shared_ptr<DevNozzleRack>& rack, const std::string& id,
                               Step step = Step::Stopped, int slot = -1)
{
    ColdPull::Reading r;
    auto* manager = wxGetApp().getDeviceManager();
    auto* m = manager ? manager->get_selected_machine() : nullptr;
    if (!m || id.empty() || m->get_dev_id() != id || m->GetNozzleRack() != rack || !m->is_connected() ||
        !SupportsRackColdPull(rack) ||
        wxGetApp().is_monitor_ctrl_blocked(MonitorCtrlId::NozzleRack) ||
        m->is_in_printing() || m->is_in_prepare() || m->is_in_upgrading() ||
        m->is_in_calibration() || m->print_error || m->mc_print_error_code ||
        m->GetFilaSystem()->GetAmsTaskRunStatus() != AMS_TASK_RUN_NORMAL)
        return r;

    const auto age = std::chrono::system_clock::now() - m->last_update_time;
    if (age < decltype(age)::zero() || age > std::chrono::seconds(15)) return r;
    auto* system = rack->GetNozzleSystem();
    const auto occupancy = system->GetReportedOccupancy();
    if (!occupancy || Clock::now() - system->GetInventoryTime() > std::chrono::seconds(15)) return r;
    const auto& print_state = m->print_status.empty() ? m->iot_print_status : m->print_status;
    if (print_state != "IDLE" && print_state != "FINISH") return r;
    const auto ams = m->ams_status_main;
    const bool rack_idle = rack->GetStatus() == DevNozzleRack::RACK_STATUS_IDLE;
    const bool switching = ams == AMS_STATUS_MAIN_SWITCH_HOTEND;
    r.exchange = rack->GetStatus() != DevNozzleRack::RACK_STATUS_UNKNOWN &&
                 (!rack_idle || switching) && (ams == AMS_STATUS_MAIN_IDLE || switching) &&
                 ColdPull::expected_exchange(step, slot, rack->IsPicking(), rack->IsPlacing(), switching,
                     system->GetReplaceNozzleSrc().value_or(-1), system->GetReplaceNozzleTar().value_or(-1));

    const auto ext = m->GetExtderSystem()->GetExtderById(MAIN_EXTRUDER_ID);
    if (!r.exchange && (!ext || !system->IsIdle() || rack->GetReadingCount() > 0)) return r;
    const auto head = system->GetExtNozzle(MAIN_EXTRUDER_ID);
    if (!r.exchange && (ext->HasNozzleInstalled() != head.IsExist() || head.IsExist() != ((*occupancy & 1) != 0))) return r;
    if (head.IsExist()) {
        if (!r.exchange && (!head.IsHotend() || !head.IsNormal() || !head.IsInfoReliable())) return r;
        r.head = head.GetReportedSerial();
        r.head_slot = head.GetAddrId().value_or(-1);
        if (r.exchange && r.head_slot < 0) r.head_slot = slot; // In flight only; the serial still has to match.
        if ((!r.exchange && r.head.empty()) || r.head_slot < 0 || r.head_slot >= 6) return r;
    }
    for (int i = 0; i < 6; ++i) {
        const auto nozzle = system->GetNozzleOptByPosId(0x10 + i);
        const bool occupied = (*occupancy & (1 << (0x10 + i))) != 0;
        if (r.exchange && i == slot) {
            if (nozzle && nozzle->IsExist()) r.dock[i] = nozzle->GetReportedSerial();
            continue;
        }
        if (occupied != (nozzle && nozzle->IsExist()) || (nozzle && nozzle->IsToolLostInHolder())) return r;
        if (!occupied) continue;
        if (!nozzle->IsInfoReliable() || !nozzle->IsNormal() || !nozzle->IsHotend()) return r;
        r.dock[i] = nozzle->GetReportedSerial();
        if (r.dock[i].empty()) return r;
    }
    r.idle = !r.exchange && rack_idle && ams == AMS_STATUS_MAIN_IDLE && !m->GetExtderSystem()->IsBusyLoading();
    r.cold_pull = rack_idle && ams == AMS_STATUS_MAIN_COLD_PULL;
    r.valid = true;
    return r;
}

wxString instruction(Step step) {
    switch (step) {
    case Step::Pick: return _L("Clear the chamber of hands and tools. Take this hotend from its rack slot.");
    case Step::Picking: return _L("Waiting for the selected hotend to be reported on the right toolhead.");
    case Step::StartPull: return _L("On the printer touchscreen, open Toolbox > Nozzle Cold Pull Maintenance. Choose the right hotend and its material. Follow the touchscreen prompts to detach the PTFE tube and insert the previously used filament. Use dedicated cleaning filament only if supported by the manufacturer's procedure for this material.");
    case Step::Pulling: return _L("Cold pull active. Follow the touchscreen prompts. When instructed, remove the filament and refit the PTFE tube. Keep clear during motion.");
    case Step::Confirm: return _L("The firmware has left cold pull mode. Check the touchscreen result. Confirm only if this nozzle finished successfully, the filament was removed and the PTFE tube was refitted. Close this guide if the operation failed or was cancelled.");
    case Step::Return: return _L("Clear the chamber of hands and tools. Return this hotend to its original rack slot.");
    case Step::Returning: return _L("Waiting for the same hotend to be reported in its original rack slot.");
    case Step::Done: return _L("Every listed hotend was confirmed by the operator and verified back in its rack slot.");
    default: return _L("Guide stopped. Check the printer touchscreen for any operation still running. Reopen the guide with an idle, connected H2C and a recognized rack inventory.");
    }
}
}

bool SupportsRackColdPull(const std::shared_ptr<DevNozzleRack>& rack)
{
    if (!rack || !rack->IsSupported() || rack->GetRackType() != DevNozzleRack::RackType_AB) return false;
    const auto* owner = rack->GetNozzleSystem()->GetOwner();
    return owner && (owner->printer_type == "O1C" || owner->printer_type == "O1C2");
}

void ShowRackColdPullDialog(wxWindow* parent, const std::shared_ptr<DevNozzleRack>& rack)
{
    auto* manager = wxGetApp().getDeviceManager();
    auto* selected = manager ? manager->get_selected_machine() : nullptr;
    if (!selected || selected->GetNozzleRack() != rack) return;
    const std::string id = selected->get_dev_id();
    ColdPull::Queue queue;
    queue.start(read_machine(rack, id));

    wxDialog dialog(parent, wxID_ANY, _L("Cold pull all rack nozzles"));
    auto* box = new wxBoxSizer(wxVERTICAL);
    auto* title = new wxStaticText(&dialog, wxID_ANY, wxEmptyString);
    auto* body = new wxStaticText(&dialog, wxID_ANY, wxEmptyString);
    auto* check = new wxCheckBox(&dialog, wxID_ANY, wxEmptyString);
    auto* action = new wxButton(&dialog, wxID_ANY, _L("Continue"));
    auto* close = new wxButton(&dialog, wxID_CANCEL, _L("Close guide"));
    for (wxWindow* w : std::array<wxWindow*, 5>{title, body, check, action, close})
        box->Add(w, 0, wxALL, dialog.FromDIP(8));
    dialog.SetSizer(box);
    auto deadline = Clock::now();
    auto render = [&] {
        const Step s = queue.step;
        title->SetLabel(queue.slot < 0 ? _L("No verified hotend queue") :
            wxString::Format(_L("Rack slot %d | %u of %u returned"), queue.slot + 1, queue.finished, queue.count));
        body->SetLabel(instruction(s));
        body->Wrap(dialog.FromDIP(430));
        const bool motion = s == Step::Pick || s == Step::Return;
        check->SetLabel(motion ? _L("Hands and tools are clear of the chamber") :
                                 _L("The printer confirmed success and the PTFE tube is refitted"));
        check->SetValue(false);
        check->Show(motion || s == Step::Confirm);
        action->SetLabel(s == Step::Pick ? _L("Take nozzle") :
                         s == Step::Return ? _L("Return nozzle") : _L("Confirm result"));
        action->Show(check->IsShown());
        action->Disable();
        dialog.Layout();
        dialog.Fit();
        deadline = Clock::now() + ((s == Step::Picking || s == Step::Returning) ?
            std::chrono::minutes(5) : std::chrono::minutes(30));
    };
    auto poll = [&] {
        const auto before = queue.step;
        if (before == Step::Done || before == Step::Stopped) return;
        if (Clock::now() >= deadline) queue.step = Step::Stopped;
        else queue.observe(read_machine(rack, id, queue.step, queue.slot));
        if (queue.step != before) render();
    };
    wxTimer timer(&dialog);
    dialog.Bind(wxEVT_TIMER, [&](wxTimerEvent&) { poll(); });
    check->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { action->Enable(check->GetValue()); });
    action->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
        const auto original = queue.step;
        auto ready = [&] {
            poll();
            return queue.step == original && check->GetValue() && action->IsEnabled();
        };
        if (!ready()) return;
        if (original == Step::Pick || original == Step::Return) {
            const int pos = 0x10 + queue.slot;
            if (!rack->CtrlNozzleExchange(original == Step::Pick ? pos : 0,
                                         original == Step::Pick ? 0 : pos, ready)) {
                queue.step = Step::Stopped;
                render();
                return;
            }
        }
        queue.confirm();
        render();
    });
    render();
    timer.Start(500);
    dialog.ShowModal();
    timer.Stop();
}
}
