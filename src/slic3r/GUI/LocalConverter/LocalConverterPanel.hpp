#pragma once
#include "libslic3r/LocalConverter/Converter.hpp"
#include <atomic>
#include <array>
#include <memory>
#include <thread>
#include <wx/panel.h>
#include <wx/timer.h>

class SearchField;
class wxListCtrl;
class wxTextCtrl;
class wxStaticText;
class wxNotebook;

namespace Slic3r::GUI {
// A real destination panel. Its owner provides the installed package receipt
// and private local data directory, then places it in the normal tab router.
class LocalConverterPanel final : public wxPanel {
public:
    LocalConverterPanel(wxWindow *, LocalConverter::PackageProof, std::filesystem::path queue_directory);
    ~LocalConverterPanel() override;
private:
    void refresh_catalog();
    void refresh_queue();
    void choose_source(bool folder);
    void admit(std::filesystem::path source, bool folder, std::vector<std::filesystem::path> additional = {});
    void run();
    void stop();
    void export_page();
    void update_status(const wxString &);
    LocalConverter::PackageProof m_proof;
    std::unique_ptr<LocalConverter::Queue> m_queue;
    std::vector<LocalConverter::Adapter> m_adapters;
    std::vector<LocalConverter::Job> m_page;
    std::array<SearchField *,8> m_search{};
    std::array<wxListCtrl *,8> m_catalogs{};
    std::array<std::vector<std::size_t>,8> m_visible;
    SearchField *m_queue_search = nullptr;
    wxNotebook *m_categories = nullptr;
    wxListCtrl *m_jobs = nullptr;
    wxTextCtrl *m_destination = nullptr;
    wxTextCtrl *m_pdf_pages = nullptr;
    wxTextCtrl *m_pdf_title = nullptr;
    int m_pdf_rotation = 90;
    wxStaticText *m_status = nullptr;
    wxStaticText *m_details = nullptr;
    wxStaticText *m_page_label = nullptr;
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
