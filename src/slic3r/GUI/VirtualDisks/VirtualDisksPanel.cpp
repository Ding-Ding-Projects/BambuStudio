#include "VirtualDisksPanel.hpp"

#include "../DocsBrowserDialog.hpp"
#include "../GUI.hpp"
#include "../GUI_App.hpp"
#include "../GUI_Utils.hpp"
#include "../I18N.hpp"
#include "../MainFrame.hpp"
#include "../MsgDialog.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/CheckBox.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3DataView.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/MD3Menu.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/SpinInput.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/SuperConfirmGate.hpp"
#include "../Widgets/TextInput.hpp"

#include "libslic3r/Utils.hpp"

#include <nlohmann/json.hpp>

#include <wx/clipbrd.h>
#include <wx/datetime.h>
#include <wx/dirdlg.h>
#include <wx/dnd.h>
#include <wx/filedlg.h>
#include <wx/menu.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/utils.h>
#include <wx/wrapsizer.h>

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace Slic3r { namespace GUI { namespace VirtualDisks {

namespace fs = std::filesystem;

namespace {

constexpr const char *kGuide = "docs/features/windows/virtual-disks.md";

enum : std::uint32_t {
    GlyphHistory = 0xE8B3,
    GlyphAttach  = 0xE226,
    GlyphFolder  = 0xE2C8,
    GlyphPack    = 0xE1A1,
    GlyphAdd     = 0xE145,
    GlyphCopy    = 0xE14D,
    GlyphRefresh = 0xE5D5,
    GlyphEject   = 0xE8FB,
    GlyphDisk    = 0xF7A4,
    GlyphGrow    = 0xF1CE,
    GlyphCompact = 0xE94D,
    GlyphChild   = 0xE0B6,
    GlyphMerge   = 0xEB98,
    GlyphLink    = 0xE157,
    GlyphId      = 0xE90D,
    GlyphRemove  = 0xE872,
    GlyphHelp    = 0xE887,
    GlyphCancel  = 0xE5C9,
};

wxColour role(MD3::Role r) { return StateColor::semantic(r); }
wxString u8(const std::string &s) { return wxString::FromUTF8(s); }
std::string leaf(const std::string &path) { return fs::u8path(path).filename().u8string(); }
std::string folder(const std::string &path) { return fs::u8path(path).parent_path().u8string(); }

Button *button(wxWindow *parent, const wxString &text, std::uint32_t glyph, Button::Variant variant = Button::Variant::Outlined)
{
    auto *b = new Button(parent, text);
    b->SetVariant(variant);
    b->SetButtonSize(Button::Size::Small);
    if (glyph) b->SetGlyph(glyph);
    return b;
}

Label *text(wxWindow *parent, const wxFont &font, const wxString &t, bool muted = false)
{
    auto *l = new Label(parent, font, t);
    l->SetForegroundColour(role(muted ? MD3::Role::OnSurfaceVariant : MD3::Role::OnSurface));
    return l;
}

wxString kind_text(const DiskInfo &d)
{
    switch (d.kind) {
    case Kind::Fixed: return _L("fixed");
    case Kind::Dynamic: return _L("dynamic");
    case Kind::Differencing: return _L("differencing");
    case Kind::ReadOnly: return _L("read-only");
    default: return _L("unknown");
    }
}

wxString chain_text(const ChainTest &t, bool has_children)
{
    switch (t.result) {
    case ChainResult::Passed: return has_children ? _L("Protected") : _L("Passed");
    case ChainResult::ParentMissing: return _L("Parent missing");
    case ChainResult::ParentChanged: return _L("Parent changed");
    case ChainResult::Loop: return _L("Loops");
    case ChainResult::Unreadable: return _L("Unreadable");
    default: return _L("Not a chain");
    }
}

std::uint64_t parse_size(const wxString &number, int unit)
{
    double v = 0;
    if (!number.ToCDouble(&v) || v <= 0) return 0;
    const std::uint64_t scale = unit == 0 ? kMB : unit == 1 ? kGB : kTB;
    return std::uint64_t(v * double(scale) / double(kMB) + 0.5) * kMB;
}

// A dialog with the kit caption and a body sizer under it.
class Dialog : public DPIDialog
{
public:
    Dialog(wxWindow *parent, const wxString &title) : DPIDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxRESIZE_BORDER)
    {
        SetBackgroundColour(role(MD3::Role::Surface));
        root = new wxBoxSizer(wxVERTICAL);
        root->Add(new MD3DialogCaption(this, title), 0, wxEXPAND);
        body = new wxBoxSizer(wxVERTICAL);
        root->Add(body, 1, wxEXPAND | wxALL, FromDIP(18));
        SetSizer(root);
    }
    void finish(const wxString &ok_label, bool ok_enabled = true)
    {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        row->AddStretchSpacer(1);
        auto *cancel = button(this, _L("Cancel"), 0);
        cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
        ok = button(this, ok_label, 0, Button::Variant::Filled);
        ok->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_OK); });
        ok->Enable(ok_enabled);
        row->Add(cancel, 0, wxRIGHT, FromDIP(8));
        row->Add(ok, 0);
        root->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(18));
        Fit();
        MD3DialogCaption::FinishChrome(this);
        CenterOnParent();
        wxGetApp().UpdateDlgDarkUI(this);
    }
    wxBoxSizer *root = nullptr, *body = nullptr;
    Button     *ok   = nullptr;

protected:
    void on_dpi_changed(const wxRect &) override { Layout(); }
};

ComboBox *choice(wxWindow *parent, const std::vector<wxString> &items, int selected, int width)
{
    auto *c = new ComboBox(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(width, -1), 0, nullptr, wxCB_READONLY);
    for (const wxString &i : items) c->Append(i);
    c->SetSelection(selected);
    return c;
}

// The create dialog: everything a new disk can be, checked as it is typed,
// with the steps it will run beside it.
class CreateDialog : public Dialog
{
public:
    CreateDialog(wxWindow *parent, const std::string &suggested_folder, const DiskInfo *parent_disk)
        : Dialog(parent, parent_disk ? _L("Make a child disk") : _L("Create a virtual disk")), m_parent_disk(parent_disk ? *parent_disk : DiskInfo())
    {
        m_child = parent_disk != nullptr;
        const int w = FromDIP(150);
        auto     *lead = text(this, Label::Body_13,
                              m_child ? _L("A child starts empty and reads everything else from its parent, which must not change while the child exists.")
                                      : _L("Checks every limit before anything is written. Windows asks for administrator permission once, at the end."),
                              true);
        lead->Wrap(FromDIP(640));
        body->Add(lead, 0, wxBOTTOM, FromDIP(12));
        auto *grid = new wxFlexGridSizer(2, FromDIP(8), FromDIP(12));
        grid->AddGrowableCol(1, 1);
        const auto row = [&](const wxString &label, wxWindow *control) {
            grid->Add(text(this, Label::Body_13, label), 0, wxALIGN_CENTER_VERTICAL);
            grid->Add(control, 1, wxEXPAND);
        };
        if (!m_child) {
            m_preset = choice(this, {_L("Portable data disk"), _L("Dev Drive"), _L("Disk of my own")}, 0, w * 2);
            row(_L("Start from"), m_preset);
            m_format = choice(this, {"VHDX", "VHD"}, 0, w);
            row(_L("Format"), m_format);
            m_type = choice(this, {_L("Dynamic"), _L("Fixed")}, 0, w);
            row(_L("Type"), m_type);
            auto *size_row = new wxBoxSizer(wxHORIZONTAL);
            m_size = new TextInput(this, "64", wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(w, -1));
            m_unit = choice(this, {"MB", "GB", "TB"}, 1, FromDIP(90));
            size_row->Add(m_size, 0, wxRIGHT, FromDIP(8));
            size_row->Add(m_unit, 0);
            grid->Add(text(this, Label::Body_13, _L("Size")), 0, wxALIGN_CENTER_VERTICAL);
            grid->Add(size_row, 0);
            m_block = choice(this, {_L("Default"), "1 MB", "8 MB", "32 MB", "256 MB"}, 0, w);
            row(_L("Block"), m_block);
            m_sectors = choice(this, {"512 / 4096", "4096 / 4096", "512 / 512"}, 0, w);
            row(_L("Sectors (logical / physical)"), m_sectors);
            m_style = choice(this, {"GPT", "MBR", _L("Leave raw")}, 0, w);
            row(_L("Initialize"), m_style);
            m_fs = choice(this, {"NTFS", "ReFS", "exFAT", "FAT32"}, 0, w);
            row(_L("File system"), m_fs);
            m_label = new TextInput(this, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(w * 2, -1));
            row(_L("Label"), m_label);
            std::vector<wxString> letters = {_L("Next free letter")};
            for (char c = 'D'; c <= 'Z'; ++c) letters.push_back(wxString::Format("%c:", c));
            m_letter = choice(this, letters, 0, w);
            row(_L("Drive letter"), m_letter);
            m_quick = new ::CheckBox(this);
            m_quick->SetValue(true);
            row(_L("Quick format"), m_quick);
        }
        auto *where = new wxBoxSizer(wxHORIZONTAL);
        const std::string start = m_child ? folder(m_parent_disk.path) + "\\" + fs::u8path(m_parent_disk.path).stem().u8string() + " child.avhdx"
                                          : suggested_folder + "\\New disk.vhdx";
        m_path = new TextInput(this, u8(m_child && m_parent_disk.format == Format::Vhd ? start.substr(0, start.size() - 6) + ".vhd" : start), wxEmptyString,
                               wxEmptyString, wxDefaultPosition, wxSize(FromDIP(420), -1));
        auto *browse = button(this, _L("Browse..."), GlyphFolder);
        browse->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            wxFileDialog d(this, _L("Where the disk goes"), wxString(), wxString(),
                           m_parent_disk.format == Format::Vhd ? "VHD (*.vhd)|*.vhd" : "VHDX (*.vhdx;*.avhdx)|*.vhdx;*.avhdx|VHD (*.vhd)|*.vhd",
                           wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
            if (d.ShowModal() == wxID_OK) {
                m_path->GetTextCtrl()->SetValue(d.GetPath());
                update();
            }
        });
        where->Add(m_path, 1, wxRIGHT, FromDIP(8));
        where->Add(browse, 0);
        grid->Add(text(this, Label::Body_13, _L("Where it goes")), 0, wxALIGN_CENTER_VERTICAL);
        grid->Add(where, 1, wxEXPAND);
        m_attach = new ::CheckBox(this);
        m_attach->SetValue(!m_child);
        row(_L("Attach after creating"), m_attach);
        body->Add(grid, 0, wxEXPAND);

        m_checks = text(this, Label::Body_12, wxString(), true);
        body->Add(m_checks, 0, wxEXPAND | wxTOP, FromDIP(12));
        auto *steps_row = new wxBoxSizer(wxHORIZONTAL);
        steps_row->Add(text(this, Label::Head_14, _L("Same steps as a script")), 1, wxALIGN_CENTER_VERTICAL);
        m_which = choice(this, {"PowerShell", "DiskPart"}, 0, FromDIP(130));
        steps_row->Add(m_which, 0, wxRIGHT, FromDIP(8));
        auto *copy = button(this, _L("Copy"), GlyphCopy);
        copy->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            if (wxTheClipboard->Open()) {
                wxTheClipboard->SetData(new wxTextDataObject(m_script->GetValue()));
                wxTheClipboard->Close();
            }
        });
        steps_row->Add(copy, 0);
        body->Add(steps_row, 0, wxEXPAND | wxTOP, FromDIP(12));
        m_script = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(640), FromDIP(150)),
                                  wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
        m_script->SetFont(Label::Mono_12);
        body->Add(m_script, 1, wxEXPAND | wxTOP, FromDIP(6));

        for (ComboBox *c : {m_preset, m_format, m_type, m_unit, m_block, m_sectors, m_style, m_fs, m_letter, m_which})
            if (c) c->Bind(wxEVT_COMBOBOX, [this, c](wxCommandEvent &) {
                if (c == m_preset) apply_preset();
                update();
            });
        for (TextInput *t : {m_size, m_label, m_path})
            if (t) t->GetTextCtrl()->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { update(); });
        for (::CheckBox *c : {m_quick, m_attach})
            if (c) c->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &e) {
                update();
                e.Skip();
            });
        finish(m_child ? _L("Make the child") : _L("Create"), false);
        update();
    }

    CreateRequest request() const
    {
        CreateRequest r;
        r.path = into_u8(m_path->GetTextCtrl()->GetValue());
        if (m_child) {
            r.kind   = Kind::Differencing;
            r.parent = m_parent_disk.path;
            r.format = m_parent_disk.format;
            r.attach = m_attach->GetValue();
            r.partition_style.clear();
            return r;
        }
        r.format          = m_format->GetSelection() == 1 ? Format::Vhd : Format::Vhdx;
        r.kind            = m_type->GetSelection() == 1 ? Kind::Fixed : Kind::Dynamic;
        r.size            = parse_size(m_size->GetTextCtrl()->GetValue(), m_unit->GetSelection());
        const std::uint32_t blocks[] = {0, 1, 8, 32, 256};
        r.block_size      = std::uint32_t(blocks[std::max(0, m_block->GetSelection())] * kMB);
        const int s       = m_sectors->GetSelection();
        r.logical_sector  = s == 1 ? 4096 : 512;
        r.physical_sector = s == 2 ? 512 : 4096;
        const int style   = m_style->GetSelection();
        r.partition_style = style == 0 ? "GPT" : style == 1 ? "MBR" : "";
        r.file_system     = into_u8(m_fs->GetStringSelection());
        r.label           = into_u8(m_label->GetTextCtrl()->GetValue());
        r.drive_letter    = m_letter->GetSelection() > 0 ? char('D' + m_letter->GetSelection() - 1) : 0;
        r.quick_format    = m_quick->GetValue();
        r.attach          = m_attach->GetValue();
        r.dev_drive       = m_preset->GetSelection() == 1;
        return r;
    }

private:
    void apply_preset()
    {
        if (m_preset->GetSelection() == 1) { // Dev Drive
            m_format->SetSelection(0), m_type->SetSelection(0), m_style->SetSelection(0), m_fs->SetSelection(1);
            m_size->GetTextCtrl()->SetValue("50"), m_unit->SetSelection(1);
            if (m_label->GetTextCtrl()->IsEmpty()) m_label->GetTextCtrl()->SetValue("DevDrive");
        } else if (m_preset->GetSelection() == 0) { // portable data disk
            m_format->SetSelection(0), m_type->SetSelection(0), m_style->SetSelection(0), m_fs->SetSelection(0);
        }
    }
    void update()
    {
        const CreateRequest r = request();
        std::error_code     ec;
        const bool          exists = !r.path.empty() && fs::exists(fs::u8path(r.path), ec);
        const Validation    v      = validate(r, free_space(r.path), exists, m_child ? &m_parent_disk : nullptr);
        wxString            lines;
        for (const std::string &e : v.errors) lines << wxUniChar(0x2716) << " " << u8(e) << "\n";
        for (const std::string &n : v.notes) lines << wxUniChar(0x2139) << " " << u8(n) << "\n";
        m_checks->SetLabel(lines.Trim());
        m_checks->SetForegroundColour(v.ok() ? role(MD3::Role::OnSurfaceVariant) : role(MD3::Role::Error));
        m_checks->Wrap(FromDIP(640));
        const Script s = create_script(r);
        m_script->SetValue(u8(m_which->GetSelection() == 1 ? (s.diskpart.empty() ? std::string("DiskPart cannot do this step.") : s.diskpart) : clean_script(s)));
        if (ok) ok->Enable(v.ok());
        Layout();
    }

    bool        m_child = false;
    DiskInfo    m_parent_disk;
    ComboBox   *m_preset = nullptr, *m_format = nullptr, *m_type = nullptr, *m_unit = nullptr, *m_block = nullptr, *m_sectors = nullptr,
               *m_style = nullptr, *m_fs = nullptr, *m_letter = nullptr, *m_which = nullptr;
    TextInput  *m_size = nullptr, *m_label = nullptr, *m_path = nullptr;
    ::CheckBox *m_quick = nullptr, *m_attach = nullptr;
    Label      *m_checks = nullptr;
    wxTextCtrl *m_script = nullptr;
};

class DropFiles : public wxFileDropTarget
{
public:
    explicit DropFiles(std::function<void(const std::vector<std::string> &)> on) : m_on(std::move(on)) {}
    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString &names) override
    {
        std::vector<std::string> paths;
        for (const wxString &n : names) paths.push_back(into_u8(n));
        m_on(paths);
        return true;
    }

private:
    std::function<void(const std::vector<std::string> &)> m_on;
};

} // namespace

// ---------------------------------------------------------------- the panel

void VirtualDisksPanel::open()
{
    if (MainFrame *frame = wxGetApp().mainframe)
        frame->open_service("virtual-disks", _L("Virtual disks"), [](wxWindow *parent) { return new VirtualDisksPanel(parent); });
}

VirtualDisksPanel::VirtualDisksPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY), m_cancel(std::make_shared<std::atomic<bool>>(false)), m_alive(std::make_shared<bool>(true))
{
    SetName("virtual-disks");
    SetBackgroundColour(role(MD3::Role::Surface));
    m_routes = find_routes();
    load();
    build();
    refresh();
    SetDropTarget(new DropFiles([this](const std::vector<std::string> &paths) { add_files(paths); }));
}

VirtualDisksPanel::~VirtualDisksPanel()
{
    *m_alive = false;
    m_cancel->store(true);
}

void VirtualDisksPanel::load()
{
    std::ifstream in(fs::u8path(data_dir()) / "virtual-disks.json", std::ios::binary);
    if (!in) return;
    std::stringstream t;
    t << in.rdbuf();
    from_json(t.str(), m_library);
}

void VirtualDisksPanel::save()
{
    std::error_code ec;
    const fs::path  file    = fs::u8path(data_dir()) / "virtual-disks.json";
    const fs::path  partial = fs::path(file).concat(".partial");
    {
        std::ofstream out(partial, std::ios::binary | std::ios::trunc);
        out << to_json(m_library);
        if (!out) return;
    }
    fs::rename(partial, file, ec);
}

void VirtualDisksPanel::build()
{
    const int pad  = FromDIP(MD3::Metrics::active().padding);
    auto     *root = new wxBoxSizer(wxVERTICAL);

    auto *head = new wxBoxSizer(wxHORIZONTAL);
    auto *titles = new wxBoxSizer(wxVERTICAL);
    titles->Add(text(this, Label::Head_20, _L("Virtual disks")), 0);
    auto *sub = text(this, Label::Body_13, _L("Every VHD, VHDX, VHD Set and ISO you add, with every operation Windows offers. Nothing here touches a disk you did not choose."), true);
    sub->Wrap(FromDIP(620));
    titles->Add(sub, 0, wxTOP, FromDIP(2));
    head->Add(titles, 1, wxALIGN_CENTER_VERTICAL);
    const auto head_button = [&](const wxString &label, std::uint32_t glyph, Button::Variant v, std::function<void()> fn) {
        auto *b = button(this, label, glyph, v);
        b->Bind(wxEVT_BUTTON, [fn](wxCommandEvent &) { fn(); });
        head->Add(b, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
        return b;
    };
    head_button(_L("History"), GlyphHistory, Button::Variant::Text, [this]() { show_history(); });
    head_button(_L("How it works"), GlyphHelp, Button::Variant::Text, []() { DocsBrowserDialog::ShowArticle(wxGetApp().mainframe, kGuide); });
    head_button(_L("Add a disk file..."), GlyphAttach, Button::Variant::Outlined, [this]() { add_file_dialog(); });
    head_button(_L("Add a folder..."), GlyphFolder, Button::Variant::Outlined, [this]() { add_folder_dialog(); });
    head_button(_L("Quick pack..."), GlyphPack, Button::Variant::Outlined, [this]() { quick_pack(); })->Enable(m_routes.windows);
    head_button(_L("Create..."), GlyphAdd, Button::Variant::Filled, [this]() { create_disk(false); })->Enable(m_routes.windows);
    root->Add(head, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    m_routes_label = text(this, Label::Body_12, wxString(), true);
    root->Add(m_routes_label, 0, wxLEFT | wxRIGHT | wxTOP, pad);
    m_system_label = text(this, Label::Body_12, wxString(), true);
    root->Add(m_system_label, 0, wxLEFT | wxRIGHT, pad);

    auto *find = new wxBoxSizer(wxHORIZONTAL);
    m_search = new TextInput(this, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(320), -1));
    m_search->GetTextCtrl()->SetHint(_L("Search name, path, label or ID"));
    m_search->GetTextCtrl()->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { refresh_list(); });
    find->Add(m_search, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
    for (int i = 0; i < 4; ++i) {
        auto *chip = button(this, wxString(), 0);
        chip->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent &) {
            m_filter = i;
            refresh_list();
        });
        m_chips.push_back(chip);
        find->Add(chip, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    }
    find->AddStretchSpacer(1);
    auto *again = button(this, _L("Read again"), GlyphRefresh, Button::Variant::Text);
    again->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { refresh(); });
    find->Add(again, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(find, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    auto *body = new wxBoxSizer(wxHORIZONTAL);
    m_list     = new MD3DataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES | wxDV_SINGLE);
    m_list->SetName(_L("Virtual disks"));
    const std::vector<std::pair<wxString, int>> columns = {{_L("Name"), 190}, {_L("Folder"), 150}, {_L("Format and type"), 150}, {_L("Size"), 80},
                                                           {_L("On disk"), 80}, {_L("Attached"), 110}, {_L("Chain test"), 110}};
    for (const auto &c : columns) m_list->AppendTextColumn(c.first, wxDATAVIEW_CELL_INERT, FromDIP(c.second), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent &) {
        const int row = m_list->GetSelectedRow();
        m_selected    = row >= 0 && row < int(m_visible.size()) ? m_rows[std::size_t(m_visible[std::size_t(row)])].info.path : std::string();
        refresh_details();
    });
    m_list->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, [this](wxDataViewEvent &) { copy_steps(m_list); });
    body->Add(m_list, 3, wxEXPAND | wxRIGHT, pad);

    auto *details = new wxBoxSizer(wxVERTICAL);
    m_title = text(this, Label::Head_16, _L("Choose a disk"));
    details->Add(m_title, 0);
    m_state = text(this, Label::Body_13, wxString(), true);
    details->Add(m_state, 0, wxTOP, FromDIP(2));
    m_problem = text(this, Label::Body_13, wxString());
    m_problem->SetForegroundColour(role(MD3::Role::Error));
    details->Add(m_problem, 0, wxTOP, FromDIP(6));
    m_facts = text(this, Label::Mono_12, wxString());
    details->Add(m_facts, 0, wxTOP, FromDIP(8));
    auto *ops = new wxWrapSizer(wxHORIZONTAL);
    const auto op = [&](const wxString &label, std::uint32_t glyph, std::function<void()> fn) {
        auto *b = button(this, label, glyph);
        b->Bind(wxEVT_BUTTON, [fn](wxCommandEvent &) { fn(); });
        m_actions.push_back(b);
        ops->Add(b, 0, wxRIGHT | wxBOTTOM, FromDIP(6));
    };
    op(_L("Attach"), GlyphAttach, [this]() { op_attach(false); });
    op(_L("Attach read-only"), GlyphAttach, [this]() { op_attach(true); });
    op(_L("Detach"), GlyphEject, [this]() { op_detach(); });
    op(_L("Grow..."), GlyphGrow, [this]() { op_grow(); });
    op(_L("Compact..."), GlyphCompact, [this]() { op_compact(); });
    op(_L("Make child..."), GlyphChild, [this]() { create_disk(true); });
    op(_L("Merge into parent..."), GlyphMerge, [this]() { op_merge(); });
    op(_L("Reconnect parent..."), GlyphLink, [this]() { op_reconnect(); });
    op(_L("New disk ID..."), GlyphId, [this]() { op_new_id(); });
    op(_L("Show in folder"), GlyphFolder, [this]() { op_show_in_folder(); });
    op(_L("Copy the steps..."), GlyphCopy, [this]() { copy_steps(m_actions.back()); });
    op(_L("Remove from the list"), GlyphRemove, [this]() { remove_selected(); });
    details->Add(ops, 0, wxEXPAND | wxTOP, FromDIP(12));
    body->Add(details, 2, wxEXPAND);
    root->Add(body, 1, wxEXPAND | wxALL, pad);

    auto *drop = text(this, Label::Body_12, _L("Drop .vhd, .vhdx, .avhdx, .vhds or .iso files anywhere on this page to add them."), true);
    root->Add(drop, 0, wxLEFT | wxRIGHT | wxBOTTOM, pad);

    m_running_bar   = new wxPanel(this);
    m_running_bar->SetBackgroundColour(role(MD3::Role::SecondaryContainer));
    auto *bar       = new wxBoxSizer(wxHORIZONTAL);
    m_running_what  = new Label(m_running_bar, Label::Head_14, wxString());
    m_running_step  = new Label(m_running_bar, Label::Body_13, wxString());
    m_cancel_button = button(m_running_bar, _L("Cancel"), GlyphCancel);
    m_cancel_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        m_cancel->store(true);
        m_running_step->SetLabel(_L("Stopping at the next step..."));
    });
    bar->Add(m_running_what, 0, wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(10));
    bar->Add(m_running_step, 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, FromDIP(10));
    bar->Add(m_cancel_button, 0, wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(8));
    m_running_bar->SetSizer(bar);
    m_running_bar->Hide();
    root->Add(m_running_bar, 0, wxEXPAND);
    SetSizer(root);

    wxString routes = _L("Routes found:");
    const auto add_route = [&routes](bool have, const wxString &name) {
        routes << "  " << (have ? wxUniChar(0x2714) : wxUniChar(0x2716)) << " " << name;
    };
    add_route(m_routes.virtdisk, "VirtDisk API");
    add_route(m_routes.storage, _L("Storage module"));
    add_route(m_routes.diskpart, "DiskPart");
    add_route(m_routes.robocopy, "Robocopy");
    routes << "   " << (m_routes.hyperv ? _L("Hyper-V module installed, not needed") : _L("Hyper-V module not installed, not needed"));
    m_routes_label->SetLabel(m_routes.windows ? routes : _L("Virtual disks need Windows. Disk files can still be read and checked here; nothing can be attached or changed."));
    m_system_label->SetLabel(m_routes.windows ? wxString::Format(_L("%s: every operation here works without Hyper-V."), u8(m_routes.windows_name)) : wxString());
}

std::vector<DiskInfo> VirtualDisksPanel::infos() const
{
    std::vector<DiskInfo> out;
    for (const Row &r : m_rows) out.push_back(r.info);
    return out;
}

void VirtualDisksPanel::refresh()
{
    bool       available = false;
    const auto attached  = attached_disks(available);
    m_attach_known       = available;
    m_rows.clear();
    for (const std::string &path : m_library.disks) {
        Row row;
        row.info = read_disk(path);
        const auto at = attached.find(path_key(path));
        if (at != attached.end()) row.attach = at->second;
        // A disk attached read-write cannot be read; show what it was when it last could.
        if (row.info.readable) m_last_read[path_key(path)] = row.info;
        else if (row.attach.attached && m_last_read.count(path_key(path))) row.info = m_last_read[path_key(path)];
        m_rows.push_back(row);
    }
    const std::vector<DiskInfo> all = infos();
    for (std::size_t i = 0; i < m_rows.size(); ++i) {
        m_rows[i].chain    = test_chain(all, i);
        m_rows[i].parent   = find_parent(all, i);
        m_rows[i].children = children_of(all, i);
    }
    for (const auto &pair : same_disk_ids(all)) {
        m_rows[pair.first].clash_with  = int(pair.second);
        m_rows[pair.second].clash_with = int(pair.first);
    }
    refresh_list();
}

void VirtualDisksPanel::refresh_list()
{
    const wxString query = m_search ? m_search->GetTextCtrl()->GetValue().Lower() : wxString();
    int            counts[4] = {0, 0, 0, 0};
    m_visible.clear();
    for (std::size_t i = 0; i < m_rows.size(); ++i) {
        const Row &r       = m_rows[i];
        const bool chained = r.info.kind == Kind::Differencing || !r.children.empty();
        const bool problem = !r.info.readable || r.clash_with >= 0 || (r.chain.result != ChainResult::Passed && r.chain.result != ChainResult::NotAChain);
        const bool in[4]   = {true, r.attach.attached, chained, problem};
        for (int k = 0; k < 4; ++k) counts[k] += in[k];
        if (!in[m_filter]) continue;
        if (!query.IsEmpty()) {
            const wxString hay = (u8(r.info.path) + " " + u8(r.info.label) + " " + u8(r.info.virtual_disk_id) + " " + u8(r.info.partition_disk_id)).Lower();
            if (!hay.Contains(query)) continue;
        }
        m_visible.push_back(int(i));
    }
    const wxString names[4] = {_L("All"), _L("Attached"), _L("In chains"), _L("Problems")};
    for (int k = 0; k < 4; ++k) {
        m_chips[std::size_t(k)]->SetLabel(wxString::Format("%s %d", names[k], counts[k]));
        m_chips[std::size_t(k)]->SetVariant(k == m_filter ? Button::Variant::Tonal : Button::Variant::Outlined);
    }
    m_list->DeleteAllItems();
    int select_row = -1;
    for (int index : m_visible) {
        const Row          &r = m_rows[std::size_t(index)];
        wxVector<wxVariant> cells;
        const int depth = [&]() {
            int d = 0;
            for (int p = r.parent; p >= 0 && d < 8; p = m_rows[std::size_t(p)].parent) ++d;
            return d;
        }();
        wxString name = u8(leaf(r.info.path));
        if (depth > 0) name = wxString(' ', depth * 2) + wxUniChar(0x21B3) + " " + name;
        cells.push_back(name);
        cells.push_back(u8(folder(r.info.path)));
        cells.push_back(u8(format_name(r.info.format)) + (r.info.kind != Kind::Unknown ? " " + wxString(wxUniChar(0x00B7)) + " " + kind_text(r.info) : wxString()));
        cells.push_back(r.info.virtual_size ? u8(size_text(r.info.virtual_size)) : wxString(wxUniChar(0x2013)));
        cells.push_back(u8(size_text(r.info.file_size)));
        wxString attached = !m_attach_known ? _L("Unknown") : !r.attach.attached ? _L("No") : r.attach.mount_points.empty() ? _L("Yes") : u8(r.attach.mount_points.front());
        cells.push_back(attached);
        cells.push_back(r.clash_with >= 0 ? _L("Same disk ID") : !r.info.readable ? _L("Unreadable") : chain_text(r.chain, !r.children.empty()));
        m_list->AppendItem(cells);
        if (r.info.path == m_selected) select_row = int(m_list->GetItemCount()) - 1;
    }
    if (select_row >= 0) m_list->SelectRow(unsigned(select_row));
    refresh_details();
    Layout();
}

const VirtualDisksPanel::Row *VirtualDisksPanel::selected() const
{
    for (const Row &r : m_rows)
        if (r.info.path == m_selected) return &r;
    return nullptr;
}

void VirtualDisksPanel::refresh_details()
{
    const Row *r = selected();
    for (Button *b : m_actions) b->Enable(false);
    if (!r) {
        m_title->SetLabel(m_rows.empty() ? _L("No disks yet") : _L("Choose a disk"));
        m_state->SetLabel(m_rows.empty() ? _L("Add a disk file or a folder, drop files here, or create a disk.") : wxString());
        m_facts->SetLabel(wxString());
        m_problem->SetLabel(wxString());
        Layout();
        return;
    }
    const DiskInfo &d = r->info;
    m_title->SetLabel(u8(leaf(d.path)));
    wxString state;
    if (!m_attach_known) state = _L("Windows could not say whether it is attached.");
    else if (r->attach.attached) state = r->attach.mount_points.empty() ? _L("Attached, without a drive letter") : wxString::Format(_L("Attached at %s"), u8(r->attach.mount_points.front()));
    else state = _L("Not attached");
    m_state->SetLabel(state);

    wxString problem;
    if (!d.readable) problem = u8(d.problem);
    else if (r->clash_with >= 0)
        problem = wxString::Format(_L("%s has the same disk ID as %s, so Windows keeps one of them offline while the other is attached. New disk ID ends that."),
                                   u8(leaf(d.path)), u8(leaf(m_rows[std::size_t(r->clash_with)].info.path)));
    else if (r->chain.result == ChainResult::ParentMissing || r->chain.result == ChainResult::ParentChanged || r->chain.result == ChainResult::Loop)
        problem = u8(r->chain.detail);
    m_problem->SetLabel(problem);
    m_problem->Wrap(FromDIP(420));

    std::ostringstream f;
    const auto line = [&f](const char *name, const std::string &value) {
        if (!value.empty()) f << name << std::string(std::max<std::size_t>(1, 20 - std::string(name).size()), ' ') << value << "\n";
    };
    line("Format, type", std::string(format_name(d.format)) + ", " + kind_name(d.kind));
    if (d.virtual_size) line("Virtual size", size_text(d.virtual_size));
    line("On its drive", size_text(d.file_size));
    if (d.block_size) line("Block size", size_text(d.block_size));
    if (d.format == Format::Vhd || d.format == Format::Vhdx) line("Sectors", std::to_string(d.logical_sector) + " logical, " + std::to_string(d.physical_sector) + " physical");
    if (!d.label.empty()) line("Label", d.label);
    if (!d.partition_style.empty()) line("Partitions", d.partition_style);
    line("Disk ID", d.partition_disk_id);
    line("Virtual disk ID", d.virtual_disk_id);
    if (r->parent >= 0) line("Parent", leaf(m_rows[std::size_t(r->parent)].info.path));
    else if (d.kind == Kind::Differencing) line("Parent (missing)", !d.parent_absolute.empty() ? d.parent_absolute : d.parent_relative);
    if (!r->children.empty()) {
        std::string kids;
        for (std::size_t c : r->children) kids += (kids.empty() ? "" : ", ") + leaf(m_rows[c].info.path);
        line("Children", kids);
    }
    for (std::size_t i = 0; i < r->attach.mount_points.size(); ++i) line(i == 0 ? "Mounted at" : "", r->attach.mount_points[i]);
    line("Path", d.path);
    m_facts->SetLabel(u8(f.str()));

    const bool win      = m_routes.windows && !m_running;
    const bool vdisk    = d.format == Format::Vhd || d.format == Format::Vhdx;
    const bool attached = r->attach.attached;
    const bool has_kids = !r->children.empty();
    // Attach, Attach read-only, Detach, Grow, Compact, Make child, Merge, Reconnect, New ID, Show in folder, Copy steps, Remove.
    const bool enable[] = {win && !attached && d.format != Format::VhdSet,
                           win && !attached && d.format != Format::VhdSet,
                           win && attached,
                           win && vdisk && d.readable && !has_kids && d.kind != Kind::Differencing,
                           win && vdisk && d.readable && d.kind != Kind::Fixed,
                           win && vdisk && d.readable && !attached,
                           win && d.kind == Kind::Differencing && r->parent >= 0 && r->chain.result == ChainResult::Passed && !attached,
                           win && d.kind == Kind::Differencing && !attached,
                           win && vdisk && d.readable && !attached && (d.partition_style == "GPT" || d.partition_style == "MBR"),
                           true,
                           vdisk || d.format == Format::Iso,
                           !m_running};
    for (std::size_t i = 0; i < m_actions.size() && i < sizeof(enable) / sizeof(enable[0]); ++i) m_actions[i]->Enable(enable[i]);
    Layout();
}

void VirtualDisksPanel::add_files(const std::vector<std::string> &paths)
{
    int added = 0;
    for (const std::string &p : paths) {
        std::error_code ec;
        if (fs::is_directory(fs::u8path(p), ec)) {
            for (const std::string &d : find_disks(p)) m_library.add_disk(d), ++added;
        } else if (format_from_extension(p) != Format::Unknown) {
            m_library.add_disk(p), ++added;
            m_selected = p;
        }
    }
    if (added == 0) {
        MessageDialog(this, _L("None of these are disk files: .vhd, .vhdx, .avhdx, .vhds or .iso."), _L("Virtual disks"), wxOK | wxICON_INFORMATION).ShowModal();
        return;
    }
    save();
    refresh();
}

void VirtualDisksPanel::add_file_dialog()
{
    wxFileDialog d(this, _L("Add disk files"), wxString(), wxString(), _L("Disk files") + " (*.vhd;*.vhdx;*.avhdx;*.vhds;*.iso)|*.vhd;*.vhdx;*.avhdx;*.vhds;*.iso",
                   wxFD_OPEN | wxFD_MULTIPLE | wxFD_FILE_MUST_EXIST);
    if (d.ShowModal() != wxID_OK) return;
    wxArrayString names;
    d.GetPaths(names);
    std::vector<std::string> paths;
    for (const wxString &n : names) paths.push_back(into_u8(n));
    add_files(paths);
}

void VirtualDisksPanel::add_folder_dialog()
{
    wxDirDialog d(this, _L("Add every disk file in a folder"));
    if (d.ShowModal() != wxID_OK) return;
    const std::string f = into_u8(d.GetPath());
    if (std::find(m_library.folders.begin(), m_library.folders.end(), f) == m_library.folders.end()) m_library.folders.push_back(f);
    add_files({f});
}

void VirtualDisksPanel::remove_selected()
{
    if (m_selected.empty()) return;
    m_library.remove_disk(m_selected);
    m_selected.clear();
    save();
    refresh();
}

void VirtualDisksPanel::show_history()
{
    Dialog d(this, _L("History"));
    auto  *lead = text(&d, Label::Body_13, _L("Every operation, the route that did it, and how it ended. An incomplete step left everything as it was."), true);
    d.body->Add(lead, 0, wxBOTTOM, d.FromDIP(10));
    auto *list = new MD3DataViewListCtrl(&d, wxID_ANY, wxDefaultPosition, wxSize(d.FromDIP(760), d.FromDIP(360)), wxDV_ROW_LINES | wxDV_SINGLE);
    for (const auto &c : std::vector<std::pair<wxString, int>>{{_L("Time"), 130}, {_L("Operation"), 240}, {_L("Route"), 150}, {_L("Result"), 90}, {_L("Detail"), 200}})
        list->AppendTextColumn(c.first, wxDATAVIEW_CELL_INERT, d.FromDIP(c.second), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    for (const HistoryEntry &h : m_library.history) {
        wxVector<wxVariant> cells;
        cells.push_back(wxDateTime(static_cast<time_t>(h.time)).Format("%Y-%m-%d %H:%M"));
        cells.push_back(u8(h.operation));
        cells.push_back(u8(h.route));
        cells.push_back(h.result == "Done" ? _L("Done") : h.result == "Incomplete" ? _L("Incomplete") : _L("Failed"));
        cells.push_back(u8(h.detail));
        list->AppendItem(cells);
    }
    d.body->Add(list, 1, wxEXPAND);
    d.finish(_L("Close"));
    d.ShowModal();
}

void VirtualDisksPanel::create_disk(bool child_of_selected)
{
    const Row *r = selected();
    if (child_of_selected && !r) return;
    std::string start = r ? folder(r->info.path) : wxGetApp().app_config->get("last_virtual_disk_folder");
    if (start.empty()) start = into_u8(wxGetHomeDir());
    CreateDialog dialog(this, start, child_of_selected ? &r->info : nullptr);
    if (dialog.ShowModal() != wxID_OK) return;
    const CreateRequest req = dialog.request();
    wxGetApp().app_config->set("last_virtual_disk_folder", folder(req.path));
    const Script s = child_of_selected ? child_script(req.parent, req.path) : create_script(req);
    m_library.add_disk(req.path);
    m_selected = req.path;
    save();
    if (child_of_selected && req.attach) {
        run(s, req.path, [this, path = req.path](const RunOutcome &o) {
            if (o.status == RunOutcome::Status::Done) run(attach_script(path, false), path);
        });
    } else {
        run(s, req.path);
    }
}

void VirtualDisksPanel::quick_pack()
{
    wxDirDialog d(this, _L("Pack a folder into a new VHDX beside it"));
    if (d.ShowModal() != wxID_OK) return;
    const std::string source = into_u8(d.GetPath());
    wxBusyCursor      wait;
    const FolderStats stats  = folder_stats(source);
    const fs::path    src    = fs::u8path(source);
    std::vector<std::string> taken;
    std::error_code          ec;
    for (const auto &e : fs::directory_iterator(src.parent_path(), ec)) taken.push_back(e.path().u8string());
    PackRequest pack;
    pack.source       = source;
    pack.target       = unique_target(src.parent_path().u8string(), src.filename().u8string(), taken);
    pack.source_bytes = stats.bytes;
    pack.source_files = stats.files;
    pack.label        = src.filename().u8string();
    const std::uint64_t size = pack_size(stats.bytes, stats.files);
    wxString message = wxString::Format(_L("Packing %s (%d files, %s) into %s, a dynamic VHDX of %s fitted to it."), u8(source), int(stats.files),
                                        u8(size_text(stats.bytes)), u8(leaf(pack.target)), u8(size_text(size)));
    if (leaf(pack.target) != pack.label + ".vhdx") message << "\n" << wxString::Format(_L("%s already exists, so the new disk is %s. Nothing is overwritten."), u8(pack.label + ".vhdx"), u8(leaf(pack.target)));
    message << "\n\n" << _L("The folder tree, times, attributes and streams (web marks included) are kept, and every file is checked by SHA-256. Your files are never moved or changed; Cancel removes only the new disk.");
    if (!stats.complete) message << "\n\n" << _L("Some folders could not be read and are left out.");
    if (free_space(pack.target) < size) {
        MessageDialog(this, wxString::Format(_L("The drive has %s free; the packed disk needs up to %s."), u8(size_text(free_space(pack.target))), u8(size_text(size))),
                      _L("Quick pack"), wxOK | wxICON_WARNING)
            .ShowModal();
        return;
    }
    MessageDialog ask(this, message, _L("Quick pack"), wxYES_NO | wxICON_QUESTION);
    ask.SetButtonLabel(wxID_YES, _L("Pack it"), true);
    ask.SetButtonLabel(wxID_NO, _L("Cancel"));
    if (ask.ShowModal() != wxID_YES) return;
    m_library.add_disk(pack.target);
    m_selected = pack.target;
    save();
    run(pack_script(pack), pack.target, [this, target = pack.target](const RunOutcome &o) {
        if (o.status != RunOutcome::Status::Done) {
            m_library.remove_disk(target);
            save();
            refresh();
        }
    });
}

void VirtualDisksPanel::copy_steps(wxWindow *anchor)
{
    const Row *r = selected();
    if (!r) return;
    const std::string &p = r->info.path;
    std::vector<std::pair<wxString, Script>> scripts = {{_L("Attach"), attach_script(p, false)}, {_L("Attach read-only"), attach_script(p, true)}, {_L("Detach"), detach_script(p)}};
    if (r->info.format == Format::Vhd || r->info.format == Format::Vhdx) {
        scripts.push_back({_L("Compact"), compact_script(p, CompactMode::Full, r->attach.attached)});
        if (r->info.virtual_size) scripts.push_back({_L("Grow by half"), grow_script(p, ((r->info.virtual_size * 3 / 2) / kMB) * kMB, r->attach.attached)});
        if (r->parent >= 0) scripts.push_back({_L("Merge into parent"), merge_script(p, m_rows[std::size_t(r->parent)].info.path, std::string())});
    }
    wxMenu menu;
    std::vector<std::string> payloads;
    for (const auto &s : scripts) {
        auto *sub = new wxMenu();
        sub->Append(wxID_HIGHEST + int(payloads.size()), _L("Copy as PowerShell"));
        payloads.push_back(clean_script(s.second));
        wxMenuItem *dp = sub->Append(wxID_HIGHEST + int(payloads.size()), _L("Copy as DiskPart"));
        dp->Enable(!s.second.diskpart.empty());
        payloads.push_back(s.second.diskpart);
        menu.AppendSubMenu(sub, s.first);
    }
    const int chosen = MD3::PopupMenuSelection(this, menu, anchor ? anchor->ClientToScreen(wxPoint(0, anchor->GetSize().y)) : wxDefaultPosition);
    const int index  = chosen - wxID_HIGHEST;
    if (index < 0 || index >= int(payloads.size()) || !wxTheClipboard->Open()) return;
    wxTheClipboard->SetData(new wxTextDataObject(u8(payloads[std::size_t(index)])));
    wxTheClipboard->Close();
}

void VirtualDisksPanel::op_attach(bool read_only)
{
    if (const Row *r = selected()) run(attach_script(r->info.path, read_only || r->info.format == Format::Iso), r->info.path);
}

void VirtualDisksPanel::op_detach()
{
    const Row *r = selected();
    if (!r) return;
    MessageDialog ask(this, wxString::Format(_L("Detach %s? Programs using its drive lose it; close their files first."), u8(leaf(r->info.path))), _L("Detach"),
                      wxYES_NO | wxICON_QUESTION);
    ask.SetButtonLabel(wxID_YES, _L("Detach"), true);
    ask.SetButtonLabel(wxID_NO, _L("Cancel"));
    if (ask.ShowModal() == wxID_YES) run(detach_script(r->info.path), r->info.path);
}

void VirtualDisksPanel::op_grow()
{
    const Row *r = selected();
    if (!r) return;
    const DiskInfo     &d   = r->info;
    const std::uint64_t now = d.virtual_size;
    Dialog              dlg(this, wxString::Format(_L("Grow %s"), u8(leaf(d.path))));
    auto *lead = text(&dlg, Label::Body_13,
                      wxString::Format(_L("Now %s. Up to %s for a %s. Grows the file and, while it is attached, extends its last volume into the new space in the same step."),
                                       u8(size_text(now)), d.format == Format::Vhd ? "2,040 GB" : "64 TB", u8(format_name(d.format))),
                      true);
    lead->Wrap(dlg.FromDIP(520));
    dlg.body->Add(lead, 0, wxBOTTOM, dlg.FromDIP(10));
    auto *row  = new wxBoxSizer(wxHORIZONTAL);
    auto *size = new TextInput(&dlg, wxString::Format("%.0f", double(std::max<std::uint64_t>(now * 3 / 2, now + kGB)) / double(kGB)), wxEmptyString, wxEmptyString,
                               wxDefaultPosition, wxSize(dlg.FromDIP(140), -1));
    auto *unit = choice(&dlg, {"MB", "GB", "TB"}, 1, dlg.FromDIP(90));
    row->Add(text(&dlg, Label::Body_13, _L("New size")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, dlg.FromDIP(10));
    row->Add(size, 0, wxRIGHT, dlg.FromDIP(8));
    row->Add(unit, 0);
    dlg.body->Add(row, 0);
    auto *check = text(&dlg, Label::Body_12, wxString(), true);
    dlg.body->Add(check, 0, wxTOP, dlg.FromDIP(8));
    auto *shrink = text(&dlg, Label::Body_12, _L("Shrinking is not built yet: it needs a verified copy first and the two-key confirmation."), true);
    dlg.body->Add(shrink, 0, wxTOP, dlg.FromDIP(6));
    dlg.finish(_L("Grow"));
    const auto update = [&]() {
        const std::uint64_t want = parse_size(size->GetTextCtrl()->GetValue(), unit->GetSelection());
        const std::uint64_t most = d.format == Format::Vhd ? 2040 * kGB : 64 * kTB;
        wxString            msg;
        if (want <= now) msg = _L("The new size must be larger than the disk is now.");
        else if (want > most) msg = wxString::Format(_L("A %s holds at most %s."), u8(format_name(d.format)), d.format == Format::Vhd ? "2,040 GB" : "64 TB");
        else if (d.kind == Kind::Fixed && free_space(d.path) < want - now) msg = _L("A fixed disk takes the new space at once, and the drive does not have it.");
        check->SetLabel(msg.IsEmpty() ? wxString::Format(_L("Grows to %s."), u8(size_text(want))) : msg);
        check->SetForegroundColour(msg.IsEmpty() ? role(MD3::Role::OnSurfaceVariant) : role(MD3::Role::Error));
        dlg.ok->Enable(msg.IsEmpty());
        dlg.Layout();
    };
    size->GetTextCtrl()->Bind(wxEVT_TEXT, [&](wxCommandEvent &) { update(); });
    unit->Bind(wxEVT_COMBOBOX, [&](wxCommandEvent &) { update(); });
    update();
    if (dlg.ShowModal() != wxID_OK) return;
    run(grow_script(d.path, parse_size(size->GetTextCtrl()->GetValue(), unit->GetSelection()), r->attach.attached), d.path);
}

void VirtualDisksPanel::op_compact()
{
    const Row *r = selected();
    if (!r) return;
    Dialog dlg(this, wxString::Format(_L("Compact %s"), u8(leaf(r->info.path))));
    auto  *lead = text(&dlg, Label::Body_13,
                      wxString::Format(_L("%s on its drive now. Compacting gives back blocks the disk no longer uses."), u8(size_text(r->info.file_size))), true);
    dlg.body->Add(lead, 0, wxBOTTOM, dlg.FromDIP(10));
    auto *mode = choice(&dlg, {_L("Full: scans for zero blocks and gives back unused ones"), _L("Quick: gives back unused blocks without scanning for zeros")}, 0,
                        dlg.FromDIP(460));
    dlg.body->Add(mode, 0);
    if (r->attach.attached) {
        auto *note = text(&dlg, Label::Body_12, _L("It is reattached read-only for the run, then put back as it was."), true);
        dlg.body->Add(note, 0, wxTOP, dlg.FromDIP(8));
    }
    auto *later = text(&dlg, Label::Body_12, _L("Retrim, Pretrimmed and Prezeroed are not built yet."), true);
    dlg.body->Add(later, 0, wxTOP, dlg.FromDIP(6));
    dlg.finish(_L("Compact"));
    if (dlg.ShowModal() != wxID_OK) return;
    run(compact_script(r->info.path, mode->GetSelection() == 1 ? CompactMode::Quick : CompactMode::Full, r->attach.attached), r->info.path);
}

void VirtualDisksPanel::op_merge()
{
    const Row *r = selected();
    if (!r || r->parent < 0) return;
    const DiskInfo   &parent = m_rows[std::size_t(r->parent)].info;
    const std::vector<std::size_t> siblings = m_rows[std::size_t(r->parent)].children;
    std::vector<wxString> affected = {u8(leaf(parent.path))};
    wxString others;
    for (std::size_t s : siblings)
        if (m_rows[s].info.path != r->info.path) {
            affected.push_back(u8(leaf(m_rows[s].info.path)));
            others << (others.IsEmpty() ? "" : ", ") << u8(leaf(m_rows[s].info.path));
        }
    const fs::path    pp     = fs::u8path(parent.path);
    const std::string backup = (pp.parent_path() / (pp.stem().u8string() + " (copy)" + pp.extension().u8string())).u8string();
    SuperConfirmGate::Spec spec;
    spec.action      = _L("Merge into the parent");
    spec.consequence = wxString::Format(_L("%s changes for good with the changes in %s."), u8(leaf(parent.path)), u8(leaf(r->info.path)));
    if (!others.IsEmpty()) spec.consequence << " " << wxString::Format(_L("%s are built on it too and stop working after the merge."), others);
    spec.consequence << " " << wxString::Format(_L("A verified copy, %s, is made first."), u8(leaf(backup)));
    spec.affected        = affected;
    spec.completed_label = _L("Merging");
    if (!SuperConfirmGate::Run(m_actions.empty() ? nullptr : m_actions[6], spec)) return;
    if (free_space(parent.path) < parent.file_size) {
        MessageDialog(this, _L("The drive has no room for the verified copy of the parent, so nothing was merged."), _L("Merge"), wxOK | wxICON_WARNING).ShowModal();
        return;
    }
    m_library.add_disk(backup);
    save();
    run(merge_script(r->info.path, parent.path, backup), r->info.path);
}

void VirtualDisksPanel::op_reconnect()
{
    const Row *r = selected();
    if (!r) return;
    wxFileDialog d(this, wxString::Format(_L("Where is the parent of %s now?"), u8(leaf(r->info.path))), wxString(), wxString(),
                   r->info.format == Format::Vhd ? "VHD (*.vhd)|*.vhd" : "VHDX (*.vhdx;*.avhdx)|*.vhdx;*.avhdx", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (d.ShowModal() != wxID_OK) return;
    const std::string parent = into_u8(d.GetPath());
    // The chosen file must be the parent this child was made from.
    std::vector<DiskInfo> pair = {read_disk(parent), r->info};
    pair[1].parent_absolute    = parent;
    if (test_chain(pair, 1).result == ChainResult::ParentChanged) {
        MessageDialog(this, wxString::Format(_L("%s is not the parent %s was made from (or it changed since), so they were not connected."), u8(leaf(parent)), u8(leaf(r->info.path))),
                      _L("Reconnect parent"), wxOK | wxICON_WARNING)
            .ShowModal();
        return;
    }
    m_library.add_disk(parent);
    save();
    run(reconnect_parent_script(r->info.path, parent), r->info.path);
}

void VirtualDisksPanel::op_new_id()
{
    const Row *r = selected();
    if (!r) return;
    MessageDialog ask(this,
                      wxString::Format(_L("Give %s a new %s disk ID? It is attached without a drive letter for the change and detached after. Its files do not change."),
                                       u8(leaf(r->info.path)), u8(r->info.partition_style)),
                      _L("New disk ID"), wxYES_NO | wxICON_QUESTION);
    ask.SetButtonLabel(wxID_YES, _L("New disk ID"), true);
    ask.SetButtonLabel(wxID_NO, _L("Cancel"));
    if (ask.ShowModal() == wxID_YES) run(new_disk_id_script(r->info.path, r->info.partition_style), r->info.path);
}

void VirtualDisksPanel::op_show_in_folder()
{
    if (const Row *r = selected()) desktop_open_any_folder(r->info.path);
}

void VirtualDisksPanel::set_running(bool running, const wxString &what)
{
    m_running = running;
    m_running_what->SetLabel(what);
    if (running) m_running_step->SetLabel(_L("Waiting for administrator permission..."));
    m_cancel_button->Enable(running);
    m_running_bar->Show(running);
    refresh_details();
    Layout();
}

void VirtualDisksPanel::run(const Script &script, const std::string &disk, std::function<void(const RunOutcome &)> after)
{
    if (m_running || !m_routes.windows) return;
    m_cancel->store(false);
    set_running(true, u8(script.summary));
    std::shared_ptr<bool>              alive  = m_alive;
    std::shared_ptr<std::atomic<bool>> cancel = m_cancel;
    const std::string                  work   = (fs::u8path(data_dir()) / "virtual-disks").u8string();
    void                              *owner  = wxGetApp().mainframe ? wxGetApp().mainframe->GetHandle() : nullptr;
    std::thread([this, script, disk, after, alive, cancel, work, owner]() {
        const RunOutcome outcome = run_elevated(
            script, work,
            [this, alive](const std::string &line) {
                wxTheApp->CallAfter([this, alive, line]() {
                    if (*alive) m_running_step->SetLabel(u8(line));
                });
            },
            cancel.get(), owner);
        wxTheApp->CallAfter([this, alive, outcome, script, disk, after]() {
            if (!*alive) return;
            HistoryEntry h;
            h.time      = std::int64_t(std::time(nullptr));
            h.operation = script.summary;
            h.disk      = leaf(disk);
            h.route     = route_name(script.route);
            switch (outcome.status) {
            case RunOutcome::Status::Done: h.result = "Done"; break;
            case RunOutcome::Status::Cancelled:
            case RunOutcome::Status::PermissionCancelled: h.result = "Incomplete"; break;
            default: h.result = "Failed"; break;
            }
            h.detail = outcome.message == "Done" ? std::string() : outcome.message;
            if (outcome.status == RunOutcome::Status::Done && !outcome.extra_json.empty()) {
                const nlohmann::json extra = nlohmann::json::parse(outcome.extra_json, nullptr, false);
                if (extra.is_object() && extra.contains("before") && extra.contains("after"))
                    h.detail = size_text(extra.value("before", std::uint64_t(0))) + " to " + size_text(extra.value("after", std::uint64_t(0)));
                else if (extra.is_object() && extra.contains("files"))
                    h.detail = std::to_string(extra.value("files", 0)) + " files, all SHA-256 verified";
                else if (extra.is_object() && extra.contains("id"))
                    h.detail = "New ID " + extra.value("id", std::string());
            }
            m_library.record(h);
            save();
            set_running(false);
            refresh();
            // Windows mounts a volume a moment after its disk arrives: look again shortly.
            if (outcome.status == RunOutcome::Status::Done)
                for (int delay : {1500, 4000})
                    CallLater(delay, [this, alive]() {
                        if (*alive && !m_running) refresh();
                    });
            if (outcome.status == RunOutcome::Status::Failed || outcome.status == RunOutcome::Status::Unavailable)
                MessageDialog(this, wxString::Format(_L("%s did not finish: %s"), u8(script.summary), u8(outcome.message)), _L("Virtual disks"), wxOK | wxICON_WARNING)
                    .ShowModal();
            else if (outcome.status == RunOutcome::Status::PermissionCancelled)
                m_state->SetLabel(_L("Permission cancelled: nothing changed, and the step shows as incomplete."));
            if (after) after(outcome);
        });
    }).detach();
}

}}} // namespace Slic3r::GUI::VirtualDisks
