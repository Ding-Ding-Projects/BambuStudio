#ifndef slic3r_GUI_Extensions_ExtensionRuntime_hpp_
#define slic3r_GUI_Extensions_ExtensionRuntime_hpp_

// Extensions inside the running app: the one Host (extension folders in the
// data folder, the built-ins from resources), a worker thread that runs one
// extension at a time so slicing and the window never wait for one, the hooks
// the app calls, and what comes back: notes as notifications, comment lines
// added to the exported G-code, a file the person chooses where to save.
// The decisions live in the wx-free ExtensionCore, ExtensionHost and
// ExtensionContext; everything here runs on the UI thread unless it says so.

#include "ExtensionContext.hpp"
#include "ExtensionHost.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <wx/string.h>

class wxWindow;

namespace Slic3r {
class Print;
namespace GUI {
class PartPlate;
namespace Extensions {

class Runtime
{
public:
    static Runtime &instance();
    // Stops the run in progress, drops the queue and joins the worker. Called
    // from GUI_App::shutdown before any window goes; safe to call twice.
    static void shutdown();

    // Loads on first use: installs or updates the built-ins, then reads every folder.
    Host &host();
    void  reload();
    std::filesystem::path folder() const;

    // ---- hooks the app calls
    // print.finished: from startup, every 3 s, the printer selected on the
    // Device page is looked at while an extension is on for the hook; a print
    // seen running that finishes, fails or is cancelled runs it once.
    void start_watching_prints();
    void print_tick();
    // After files were loaded and objects arrived (indexes into the model):
    // runs model.imported with only those objects and the files' names.
    void models_imported(const std::vector<std::string> &files, const std::vector<std::size_t> &objects);
    // After a plate sliced. Runs once per slice result: exporting a plate that
    // is already sliced does not count as slicing it again.
    void plate_sliced(PartPlate *plate, const Print *print);
    // The background process exported plain G-code to path. The extensions run
    // once the slicing job has finished with the file (see process_completed).
    void export_finished(const std::string &path);
    void process_completed(bool success, PartPlate *plate, const Print *print);

    // ---- before a print is sent (print.before_send)
    // The runs it takes; empty when no extension is on for it.
    std::vector<PlannedRun> plan_before_send();
    // The request for a send: the plate (every sliced plate added up when all
    // are sent), the printer and the send section; subject names the plate.
    nlohmann::json before_send_context(const SendFacts &send, std::string &subject);
    // Runs them ahead of anything waiting, one at a time. each() gets every
    // result on the UI thread and done() comes once after the last, also when
    // they are stopped. Their answers and failures are the caller's to show,
    // never notifications. Returns what stop() takes.
    std::shared_ptr<std::atomic<bool>> run_before_send(std::vector<PlannedRun> runs, nlohmann::json context, std::string subject,
                                                       std::function<void(const RunResult &)> each, std::function<void()> done);
    // Stops what run_before_send started: the run in progress ends within a
    // tenth of a second and is recorded as stopped by you; a job still waiting
    // is dropped (its done() still comes).
    void stop(const std::shared_ptr<std::atomic<bool>> &cancel);

    // ---- commands
    // Runs an extension's own command with what the app has now. False when it
    // is not on, or the command is unknown.
    bool run_action(const std::string &id, const std::string &action);
    // Runs one hook with a request made of invented values marked as samples.
    // Its notes show; comment lines and files are reported, never applied.
    bool try_sample(const std::string &id, Hook hook, const std::string &action = std::string());
    void stop_all();

    // ---- what is happening, for the manager
    struct Activity
    {
        std::string running;  // extension id running now, empty when idle
        std::string hook;     // its hook id, or "command:<action>"
        int         queued = 0;
    };
    Activity activity() const;

    // For the Local tools page. Nothing is read while no consent prompt was
    // ever answered (known stays false), as at startup.
    struct Summary
    {
        bool known   = false;
        int  on      = 0;
        int  waiting = 0; // new or changed, waiting for an answer
    };
    Summary summary();

    // Panels refresh when something changes: a run started or finished, the list changed.
    int  subscribe(std::function<void()> changed);
    void unsubscribe(int token);
    void notify_changed();

    // The commands of every extension that is on, for the command palette.
    struct PaletteAction
    {
        std::string id, action, title, extension;
    };
    std::vector<PaletteAction> palette_actions();

    // Opens File > Extensions, with one extension selected when id is set.
    static void open_manager(const std::string &select = std::string());

    // What the app has now, for a hook. Every section is built; the request
    // keeps only those the extension may receive.
    nlohmann::json current_context(Hook hook, std::string &subject);

private:
    Runtime() = default;
    ~Runtime();
    Runtime(const Runtime &)            = delete;
    Runtime &operator=(const Runtime &) = delete;

    struct Job
    {
        Hook                               hook = Hook::Command;
        std::vector<PlannedRun>            runs;
        nlohmann::json                     context;
        std::string                        subject;
        std::filesystem::path              gcode;   // gcode.exported: the file to add comment lines to
        bool                               sample = false;
        bool                               first  = false; // ahead of every job waiting (print.before_send)
        std::shared_ptr<std::atomic<bool>> cancel;
        std::function<void(const RunResult &)> each; // set: results go here instead of notifications
        std::function<void()>                  done; // after the last run, or when the job is dropped
    };

    void submit(Job job);
    void ensure_worker();
    void worker_loop();
    // Worker thread: queues fn on the UI thread unless the app is closing.
    void post(std::function<void()> fn);
    // Empties the queue (m_mutex held) and returns the done() of every job dropped.
    std::vector<std::function<void()>> drop_queue(const std::shared_ptr<std::atomic<bool>> &only = nullptr);
    void apply(const RunResult &result, bool sample, const std::function<void(const RunResult &)> &each);
    void apply_comment_lines(const std::string &file, int extensions, bool ok, const std::string &problem);
    void stop_worker();

    // print.finished
    struct PrintWatch;
    std::unique_ptr<PrintWatch>            m_print_watch;

    std::unique_ptr<Host>                  m_host;
    std::map<int, std::size_t>             m_sliced_stamp; // plate index -> slice timestamp already run
    std::filesystem::path                  m_pending_export;
    std::map<int, std::function<void()>>   m_listeners;
    int                                    m_next_token = 1;

    mutable std::mutex                     m_mutex;
    std::condition_variable                m_wake;
    std::deque<Job>                        m_queue;
    std::thread                            m_worker;
    bool                                   m_stopping = false;
    std::shared_ptr<std::atomic<bool>>     m_current_cancel;
    std::string                            m_running, m_running_hook;
    std::atomic<bool>                      m_shut_down{false};
};

// One run's result in a few words, for notifications and the Runs tab.
wxString outcome_text(Outcome outcome, const std::string &problem, const Limits &limits);

}}} // namespace Slic3r::GUI::Extensions

#endif // slic3r_GUI_Extensions_ExtensionRuntime_hpp_
