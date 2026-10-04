#ifndef slic3r_SettingsDraftPanel_hpp_
#define slic3r_SettingsDraftPanel_hpp_
#include "SettingsDraftStore.hpp"
#include <wx/panel.h>
#include <functional>
#include <memory>
namespace Slic3r { namespace GUI {
class TabStrip;
class ConfigOptionsGroup;
// A detached editor. The host supplies page visibility, never preset globals.
class SettingsDraftPanel : public wxPanel {
public:
    SettingsDraftPanel(wxWindow *parent, TabStrip *strip, std::function<void(bool)> show_page);
    bool Activate(const std::string &id);
private:
    SettingsDraftStore m_store;
    TabStrip *m_strip;
    std::function<void(bool)> m_show_page;
    std::string m_active;
    std::string m_storage_key;
    size_t m_view_generation = 0;
    std::vector<std::shared_ptr<ConfigOptionsGroup>> m_groups;
    void Persist();
    void Rebuild();
    void OpenPicker();
    void Apply();
    void SaveAs();
    void UndoApply();
    DynamicPrintConfig m_undo_before, m_undo_after;
    SettingsDraftStore::Target m_undo_target;
    Preset::Type m_undo_type = Preset::TYPE_INVALID;
    SettingsDraftStore::Target Target(Preset::Type type) const;
};
}}
#endif
