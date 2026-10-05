#pragma once

#include "SchoolStore.hpp"
#include <wx/fswatcher.h>
#include <wx/timer.h>
#include <memory>

namespace Slic3r::GUI::PersonalModes {
// GUI-thread lifetime owner. The 500 ms poll recovers missed filesystem events,
// record creation/deletion, and a watch lost when a parent directory disappears.
class SchoolRuntime : public wxEvtHandler {
public:
    using Callback = std::function<void(const SchoolRecord&, RecordStatus)>;
    explicit SchoolRuntime(Callback callback)
        : m_store(SchoolStore::default_path()), m_timer(this),
          m_mode({[this] { return m_store.read(); },
                  [this](const SchoolRecord& record, std::uint64_t revision, const std::function<bool(const std::string&)>& verify) {
                      return m_store.compare_exchange(record, revision, verify);
                  }, std::move(callback)}) {
        Bind(wxEVT_TIMER, [this](wxTimerEvent&) { m_mode.reload(); });
        m_timer.Start(500);
        const auto directory = SchoolStore::default_path().parent_path();
        std::error_code ec;
        if (!directory.empty()) std::filesystem::create_directories(directory, ec);
        if (!ec && !directory.empty()) {
            m_watcher = std::make_unique<wxFileSystemWatcher>();
            m_watcher->SetOwner(this);
            Bind(wxEVT_FSWATCHER, [this](wxFileSystemWatcherEvent& event) {
                if (event.GetChangeType() & (wxFSW_EVENT_ERROR | wxFSW_EVENT_WARNING)) m_watching = false;
                m_mode.reload();
            });
            m_watching = m_watcher->Add(wxFileName(wxString(directory.wstring())));
        }
    }
    ~SchoolRuntime() override { m_timer.Stop(); m_watcher.reset(); }
    SchoolMode& mode() { return m_mode; }
    bool native_watch_available() const { return m_watching; }
    bool polling_available() const { return m_timer.IsRunning(); }
private:
    SchoolStore m_store;
    wxTimer m_timer;
    SchoolMode m_mode;
    std::unique_ptr<wxFileSystemWatcher> m_watcher;
    bool m_watching = false;
};
}
