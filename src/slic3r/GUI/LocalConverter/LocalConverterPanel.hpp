#pragma once
#include "libslic3r/LocalConverter/Converter.hpp"
#include <atomic>
#include <array>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <wx/panel.h>
#include <wx/timer.h>

class Button;
class LabeledCheckBox;
class Label;
class MD3DataViewListCtrl;
class SearchField;
class TextInput;
class wxBoxSizer;
class wxSimplebook;

namespace Slic3r::GUI {
class TabStrip;
namespace I18N { struct LocalizedText; }
// A real destination panel. Its owner provides the installed package receipt
// and private local data directory, then places it in the normal tab router.
// Every control is a registered kit primitive: the eight categories ride a
// persisted TabStrip, catalogues and the queue are MD3 tables, fields are
// TextInput and every line of text is a Label.
// Registry text from libslic3r (adapter names, disclosures, reasons, states and
// result codes) is translated here at display time; status, detail, page and
// empty-state lines render both languages themselves in bilingual mode, and
// their non-factual voice follows each language's funny level.
class LocalConverterPanel final : public wxPanel {
public:
    LocalConverterPanel(wxWindow *, LocalConverter::PackageProof, std::filesystem::path queue_directory);
    ~LocalConverterPanel() override;
private:
    void place_categories();
    void show_category(const std::string &id);
    void select_adapter(unsigned category);
    void set_rotation(int degrees);
    void update_acknowledgement();
    void refresh_catalog();
    void refresh_queue();
    void choose_source(bool folder);
    void admit(std::filesystem::path source, bool folder, std::vector<std::filesystem::path> additional = {});
    void run();
    void stop();
    void export_page();
    void update_status(const I18N::LocalizedText &);
    LocalConverter::PackageProof m_proof;
    std::unique_ptr<LocalConverter::Queue> m_queue;
    std::vector<LocalConverter::Adapter> m_adapters;
    std::vector<LocalConverter::Job> m_page;
    std::array<SearchField *,8> m_search{};
    std::array<MD3DataViewListCtrl *,8> m_catalogs{};
    std::array<Label *,8> m_empty{};
    std::array<std::vector<std::size_t>,8> m_visible;
    std::array<Button *,4> m_rotation{};
    SearchField *m_queue_search = nullptr;
    TabStrip *m_categories = nullptr;
    wxSimplebook *m_category_pages = nullptr;
    wxBoxSizer *m_category_layout = nullptr;
    MD3DataViewListCtrl *m_jobs = nullptr;
    TextInput *m_destination = nullptr;
    TextInput *m_pdf_pages = nullptr;
    TextInput *m_pdf_title = nullptr;
    Label *m_rotation_label = nullptr;
    int m_pdf_rotation = 90;
    Label *m_status = nullptr;
    Label *m_details = nullptr;
    // Explicit consent for lossy and metadata/encoding-changing adapters, kept
    // as acknowledgement tokens for this session only.
    LabeledCheckBox *m_acknowledge = nullptr;
    std::set<std::string> m_acknowledged;
    Label *m_page_label = nullptr;
    std::vector<std::size_t> m_visible_jobs;
    std::thread m_work;
    std::atomic<bool> m_cancel{false},m_running{false};
    std::atomic<bool> m_catalog_pending{false};
    std::mutex m_catalog_mutex;
    std::vector<LocalConverter::Adapter> m_next_adapters;
    std::atomic<std::uint64_t> m_admitted{0},m_rejected{0};
    std::atomic<std::uint64_t> m_preview_bytes{0};
    std::atomic<int> m_preview_kind{-1};
    std::uint64_t m_after = 0;
    std::size_t m_selected = static_cast<std::size_t>(-1);
    wxTimer m_timer;
};
} // namespace Slic3r::GUI
