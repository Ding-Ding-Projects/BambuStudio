#include "SettingContextMenu.hpp"

#include "Field.hpp"
#include "GUI.hpp"
#include "GUI_App.hpp"
#include "I18N.hpp"
#include "MainFrame.hpp"
#include "OG_CustomCtrl.hpp"
#include "OptionsGroup.hpp"
#include "Widgets/ComboBox.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3Menu.hpp"
#include "Widgets/MD3MenuLive.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/RoundedPopupWindow.hpp"
#include "Widgets/StateColor.hpp"

#include "libslic3r/Config.hpp"
#include "libslic3r/Preset.hpp"

#include <wx/clipbrd.h>
#include <wx/menu.h>
#include <wx/sizer.h>

#include <algorithm>
#include <functional>
#include <map>
#include <utility>
#include <vector>

namespace Slic3r { namespace GUI { namespace SettingContextMenu {

namespace {

wxColour role(MD3::Role r) { return StateColor::semantic(r); }

const DynamicPrintConfig *config_of(OptionsGroup *group)
{
    auto *cog = dynamic_cast<ConfigOptionsGroup *>(group);
    return cog ? cog->get_config() : nullptr;
}

wxString setting_label(const ConfigOptionDef &def)
{
    if (!def.label.empty()) return _(def.label);
    if (!def.full_label.empty()) return _(def.full_label);
    return wxString::FromUTF8(def.opt_key);
}

wxString unit_of(const ConfigOptionDef &def)
{
    if (def.type == coPercent) return "%";
    return def.sidetext.empty() ? wxString() : " " + _(def.sidetext);
}

// A setting's value in a config, as the row would show it.
wxString value_display(const ConfigOptionDef &def, const DynamicPrintConfig &config, const std::string &key)
{
    const ConfigOption *opt = config.option(key);
    if (!opt) return wxString();
    switch (def.type) {
    case coBool: return opt->getBool() ? _L("On") : _L("Off");
    case coEnum: {
        const std::string value = opt->serialize();
        for (std::size_t i = 0; i < def.enum_values.size() && i < def.enum_labels.size(); ++i)
            if (def.enum_values[i] == value) return _(def.enum_labels[i]);
        return wxString::FromUTF8(value);
    }
    case coPercent: return wxString::FromUTF8(opt->serialize());
    default: return wxString::FromUTF8(opt->serialize()) + unit_of(def);
    }
}

// Where a stepper may go: the setting's own limits, or room around the value
// when the setting has none.
std::pair<double, double> range_of(const ConfigOptionDef &def, double value)
{
    double lo = def.min, hi = def.max;
    if (!(lo > -1e7)) lo = std::min(0.0, value);
    if (!(hi < 1e7)) hi = std::max({value * 10.0, value + 100.0, lo + 100.0});
    return {lo, hi};
}

// Hands the field's value to its owner exactly as an edit in the field would.
void report(OptionsGroup *group, const std::string &key)
{
    if (Field *f = group->get_field(key); f && f->m_on_change) f->m_on_change(f->m_opt_id, f->get_value());
}

const ConfigOption *option_of(OptionsGroup *group, const std::string &key)
{
    const DynamicPrintConfig *config = config_of(group);
    return config ? config->option(key) : nullptr;
}

// A plain row whose action runs once the menu has closed.
wxMenuItem *add_action(wxMenu &menu, const wxString &label, const wxString &help, std::function<void()> action)
{
    wxMenuItem *item = menu.Append(wxID_ANY, label, help);
    menu.Bind(wxEVT_MENU, [action](wxCommandEvent &) { action(); }, item->GetId());
    return item;
}

// The value as a live control; false when the setting has none (text, lists).
bool append_value_control(wxMenu &menu, OptionsGroup *group, const std::string &key)
{
    Field             *field  = group->get_field(key);
    const ConfigOption *option = option_of(group, key);
    if (!field || !option) return false;
    const ConfigOptionDef &def   = field->m_opt;
    const wxString         label = setting_label(def);
    const wxString         help  = def.tooltip.empty() ? wxString() : _(def.tooltip);
    switch (def.type) {
    case coBool:
        if (!dynamic_cast<CheckBox *>(field)) return false;
        MD3::AppendSwitch(
            &menu, label, [group, key]() { const ConfigOption *o = option_of(group, key); return o && o->getBool(); },
            [group, key](bool on) {
                if (Field *f = group->get_field(key)) {
                    f->set_value(boost::any(on), false);
                    report(group, key);
                }
            },
            help);
        return true;
    case coInt: {
        if (!dynamic_cast<SpinCtrl *>(field)) return false;
        const auto [lo, hi] = range_of(def, option->getInt());
        MD3::AppendStepper(
            &menu, label, lo, hi, 1, 0, unit_of(def),
            [group, key]() { const ConfigOption *o = option_of(group, key); return o ? double(o->getInt()) : 0.0; },
            [group, key](double v) {
                if (Field *f = group->get_field(key)) {
                    f->set_value(boost::any(int(std::lround(v))), false);
                    report(group, key);
                }
            },
            help);
        return true;
    }
    case coFloat:
    case coPercent: {
        if (!dynamic_cast<TextCtrl *>(field)) return false;
        const double value    = option->getFloat();
        const double step     = MD3::Menu::step_for(value, false);
        const int    decimals = MD3::Menu::decimals_for(step);
        const auto [lo, hi]   = range_of(def, value);
        MD3::AppendStepper(
            &menu, label, lo, hi, step, decimals, unit_of(def),
            [group, key]() { const ConfigOption *o = option_of(group, key); return o ? o->getFloat() : 0.0; },
            [group, key, decimals](double v) {
                if (Field *f = group->get_field(key)) {
                    f->set_value(boost::any(double_to_string(v, decimals)), false);
                    report(group, key);
                }
            },
            help);
        return true;
    }
    case coEnum: {
        auto *combo = dynamic_cast<::ComboBox *>(field->getWindow());
        const t_config_enum_values *map = def.enum_keys_map;
        if (!combo || !map || def.enum_values.empty() || combo->GetCount() != def.enum_values.size()) return false;
        std::vector<wxString> labels;
        std::vector<int>      values;
        for (std::size_t i = 0; i < def.enum_values.size(); ++i) {
            const auto it = map->find(def.enum_values[i]);
            if (it == map->end()) return false;
            values.push_back(it->second);
            labels.push_back(i < def.enum_labels.size() ? _(def.enum_labels[i]) : wxString::FromUTF8(def.enum_values[i]));
        }
        const auto index_now = [group, key, values]() {
            const ConfigOption *o = option_of(group, key);
            const auto          it = o ? std::find(values.begin(), values.end(), o->getInt()) : values.end();
            return it == values.end() ? 0 : int(it - values.begin());
        };
        const auto choose = [group, key](int index) {
            if (Field *f = group->get_field(key))
                if (auto *c = dynamic_cast<::ComboBox *>(f->getWindow()); c && index >= 0 && index < int(c->GetCount())) {
                    c->SetSelection(index);
                    report(group, key);
                }
        };
        int width = 0;
        for (const wxString &l : labels) width += int(l.length());
        if (labels.size() <= 4 && width <= 28) {
            MD3::AppendChoice(&menu, label, labels, index_now, choose, help);
            return true;
        }
        // Longer lists: a submenu with the current choice marked.
        auto     *sub = new wxMenu();
        const int now = index_now();
        for (std::size_t i = 0; i < labels.size(); ++i) {
            wxMenuItem *item = sub->AppendRadioItem(wxID_ANY, labels[i]);
            sub->Bind(wxEVT_MENU, [choose, i](wxCommandEvent &) { choose(int(i)); }, item->GetId());
            item->Check(int(i) == now);
        }
        menu.AppendSubMenu(sub, label + ": " + labels[std::size_t(now)], help);
        return true;
    }
    default: return false;
    }
}

} // namespace

void explain(wxWindow *owner, OptionsGroup *group, const std::string &key, const wxPoint &screen_pos)
{
    Field *field = group ? group->get_field(key) : nullptr;
    if (!owner || !field) return;
    const ConfigOptionDef &def     = field->m_opt;
    const wxColour         surface = role(MD3::Role::SurfaceContainerHigh);
    auto                  *popup   = new RoundedPopupWindow(owner, owner->FromDIP(12));
    popup->SetBackgroundColour(surface);
    auto     *sizer = new wxBoxSizer(wxVERTICAL);
    const int pad   = owner->FromDIP(14);
    const int wrap  = owner->FromDIP(380);
    const auto add  = [&](const wxFont &font, const wxString &text, MD3::Role color) {
        auto *l = new Label(popup, font, text);
        l->SetBackgroundColour(surface);
        l->SetForegroundColour(role(color));
        l->Wrap(wrap);
        sizer->Add(l, 0, wxLEFT | wxRIGHT | wxTOP, pad);
    };
    add(Label::Head_14, setting_label(def), MD3::Role::OnSurface);
    add(Label::Body_13, def.tooltip.empty() ? _L("This setting has no description yet.") : _(def.tooltip), MD3::Role::OnSurface);
    wxString facts = wxString::Format(_L("Setting key: %s"), wxString::FromUTF8(key));
    if (def.min > -1e7 || def.max < 1e7) {
        const wxString lo = def.min > -1e7 ? double_to_string(def.min) : wxString(wxUniChar(0x2212)) + wxUniChar(0x221E);
        const wxString hi = def.max < 1e7 ? double_to_string(def.max) : wxString(wxUniChar(0x221E));
        facts << "\n" << wxString::Format(_L("Range: %s to %s"), lo, hi) << unit_of(def);
    } else if (!unit_of(def).IsEmpty()) {
        facts << "\n" << wxString::Format(_L("Unit: %s"), unit_of(def).Strip(wxString::both));
    }
    if (const DynamicPrintConfig *config = config_of(group); config && config->option(key))
        facts << "\n" << wxString::Format(_L("Now: %s"), value_display(def, *config, key));
    if (auto *cog = dynamic_cast<ConfigOptionsGroup *>(group); cog && cog->m_get_initial_config) {
        const DynamicPrintConfig initial = cog->m_get_initial_config();
        if (initial.option(key)) facts << "\n" << wxString::Format(_L("In the saved preset: %s"), value_display(def, initial, key));
    }
    add(Label::Body_12, facts, MD3::Role::OnSurfaceVariant);
    sizer->AddSpacer(pad);
    popup->SetSizer(sizer);
    popup->Finalize();
    popup->Position(screen_pos, wxSize(0, 0));
    popup->Popup();
}

namespace {

// Every row of a setting's menu, appended to `menu`.
void append_setting_items(wxMenu &menu, wxWindow *owner, OptionsGroup *group, const std::string &key, const wxPoint &screen_pos)
{
    Field *field = group ? group->get_field(key) : nullptr;
    if (!owner || !field) return;
    const ConfigOptionDef &def = field->m_opt;
    if (!append_value_control(menu, group, key)) menu.Append(wxID_ANY, setting_label(def))->Enable(false);
    menu.AppendSeparator();

    auto *cog = dynamic_cast<ConfigOptionsGroup *>(group);
    // Back to the value saved in the preset, naming it.
    wxString back_label = _L("Back to the preset value");
    if (cog && cog->m_get_initial_config) {
        const DynamicPrintConfig initial = cog->m_get_initial_config();
        if (initial.option(key)) back_label = wxString::Format(_L("Back to the preset value, %s"), value_display(def, initial, key));
    }
    add_action(menu, back_label, _L("Undo your change to this setting"), [group, key]() {
        if (Field *f = group->get_field(key)) f->on_back_to_initial_value();
    })->Enable(field->m_is_modified_value);
    if (cog && cog->m_get_sys_config && (!cog->have_sys_config || cog->have_sys_config())) {
        wxString                 sys_label = _L("Back to the system value");
        const DynamicPrintConfig sys       = cog->m_get_sys_config();
        if (sys.option(key)) sys_label = wxString::Format(_L("Back to the system value, %s"), value_display(def, sys, key));
        add_action(menu, sys_label, _L("Use the value of the system preset this one is based on"), [group, key]() {
            if (Field *f = group->get_field(key)) f->on_back_to_sys_value();
        })->Enable(field->m_is_nonsys_value);
    }
    add_action(menu, _L("Copy the setting key"), wxString::FromUTF8(key), [key]() {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(key)));
            wxTheClipboard->Close();
        }
    });
    if (cog && wxGetApp().mainframe) {
        const int type = cog->config_type();
        add_action(menu, _L("Compare with other presets..."), _L("Open the preset comparison for this kind of preset"), [type]() {
            const bool known = type == Preset::TYPE_PRINT || type == Preset::TYPE_FILAMENT || type == Preset::TYPE_PRINTER;
            if (wxGetApp().mainframe) wxGetApp().mainframe->diff_dialog.show(known ? Preset::Type(type) : Preset::TYPE_INVALID);
        });
    }
    wxWeakRef<wxWindow> anchor(owner);
    add_action(menu, _L("Explain this") + "\tF1", _L("What this setting does, its key, range and values"), [anchor, group, key, screen_pos]() {
        if (anchor) explain(anchor.get(), group, key, screen_pos);
    });
}

} // namespace

void show(wxWindow *owner, OptionsGroup *group, const std::string &key, const wxPoint &screen_pos)
{
    if (!owner || !group || !group->get_field(key)) return;
    wxMenu menu;
    append_setting_items(menu, owner, group, key, screen_pos);
    MD3::PopupMenu(owner, &menu, screen_pos);
}

void install()
{
    static bool installed = false;
    if (installed) return;
    installed = true;
    MD3::AddTextMenuSection([](wxWindow *text_window, wxMenu &menu) {
        for (wxWindow *w = text_window; w; w = w->GetParent()) {
            auto *ctrl = dynamic_cast<OG_CustomCtrl *>(w);
            if (!ctrl) continue;
            const std::string key = ctrl->option_at(wxPoint(-1, -1), text_window);
            if (key.empty() || !ctrl->opt_group) return;
            menu.AppendSeparator();
            append_setting_items(menu, text_window, ctrl->opt_group, key, wxGetMousePosition());
            return;
        }
    });
}

}}} // namespace Slic3r::GUI::SettingContextMenu
