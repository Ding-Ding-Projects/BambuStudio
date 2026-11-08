#include "ExtensionRuntime.hpp"
#include "ExtensionsPanel.hpp"

#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Preset.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/Utils.hpp"

#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include "slic3r/GUI/NotificationManager.hpp"
#include "slic3r/GUI/PartPlate.hpp"
#include "slic3r/GUI/Plater.hpp"
#include "slic3r/GUI/Selection.hpp"

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include <wx/filedlg.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>

namespace Slic3r { namespace GUI { namespace Extensions {

namespace fs = std::filesystem;
using json   = nlohmann::json;

namespace {

SliceFacts facts_for(PartPlate *plate, const Print *print, const GCodeProcessorResult *result)
{
    SliceFacts f;
    if (!plate) return f;
    f.plate      = plate->get_index() + 1;
    f.plate_name = plate->get_plate_name();
    if (result) {
        const auto &stats = result->print_statistics;
        const auto &mode  = stats.modes[std::size_t(PrintEstimatedStatistics::ETimeMode::Normal)];
        f.seconds          = mode.time;
        f.prepare_seconds  = mode.prepare_time;
        f.layers           = int(mode.layers_times.size());
        f.filament_changes = int(stats.total_filament_changes);
        for (const auto &[id, volume] : stats.total_volumes_per_extruder) {
            FilamentUse use;
            use.slot       = int(id) + 1;
            use.volume_mm3 = volume;
            if (print) {
                const PrintConfig &c = print->config();
                if (id < c.filament_type.values.size()) use.type = c.filament_type.values[id];
                if (id < c.filament_colour.values.size()) use.color = c.filament_colour.values[id];
                if (id < c.filament_density.values.size()) use.density = c.filament_density.values[id];
                if (id < c.filament_diameter.values.size()) use.diameter = c.filament_diameter.values[id];
                if (id < c.filament_cost.values.size()) use.cost_per_kg = c.filament_cost.values[id];
            }
            f.filaments.push_back(use);
        }
        for (const auto &w : result->warnings) {
            if (f.warnings.size() >= 20) break;
            f.warnings.push_back({w.level, w.error_code, w.msg, 0});
        }
    }
    BedType bed = plate->get_bed_type();
    if (bed == btDefault && wxGetApp().preset_bundle)
        if (const auto *opt = wxGetApp().preset_bundle->project_config.option<ConfigOptionEnum<BedType>>("curr_bed_type")) bed = opt->value;
    f.bed_type = bed_type_to_gcode_string(bed);
    return f;
}

PrinterFacts printer_facts()
{
    PrinterFacts  p;
    PresetBundle *bundle = wxGetApp().preset_bundle;
    if (!bundle) return p;
    const Preset &printer = bundle->printers.get_edited_preset();
    p.preset              = printer.name;
    if (const auto *model = printer.config.option<ConfigOptionString>("printer_model")) p.model = model->value;
    if (const auto *nozzles = printer.config.option<ConfigOptionFloats>("nozzle_diameter")) p.nozzles = nozzles->values;
    p.filament_presets = bundle->filament_presets;
    return p;
}

std::vector<ModelObjectFacts> model_facts(int &plate)
{
    std::vector<ModelObjectFacts> out;
    Plater                       *plater = wxGetApp().plater();
    if (!plater) return out;
    const Model     &model   = plater->model();
    const auto      &content = plater->get_selection().get_content();
    for (std::size_t i = 0; i < model.objects.size() && out.size() < 500; ++i) {
        const ModelObject *o = model.objects[i];
        ModelObjectFacts   f;
        f.name      = o->name;
        f.instances = int(o->instances.size());
        f.triangles = o->facets_count();
        if (!o->instances.empty()) {
            const Vec3d size = o->instance_bounding_box(0).size();
            f.size           = {{size.x(), size.y(), size.z()}};
        }
        f.selected = content.count(int(i)) > 0;
        out.push_back(std::move(f));
    }
    plate = plater->get_partplate_list().get_curr_plate_index() + 1;
    return out;
}

std::string plate_subject(const SliceFacts &f)
{
    std::string subject = (boost::format(_u8L("Plate %1%")) % f.plate).str();
    if (!f.plate_name.empty()) subject += " \xC2\xB7 " + f.plate_name;
    return subject;
}

NotificationManager *notifications()
{
    Plater *plater = wxGetApp().plater();
    return plater ? plater->get_notification_manager() : nullptr;
}

std::string join(const std::vector<std::string> &lines, const char *between)
{
    std::string out;
    for (const std::string &l : lines) {
        if (!out.empty()) out += between;
        out += l;
    }
    return out;
}

void save_answer_file(const std::string &extension, const Answer &answer)
{
    const wxString name = wxString::FromUTF8(answer.file_name);
    const wxString ext  = name.AfterLast('.');
    const wxString dir  = wxGetApp().app_config ? wxString::FromUTF8(wxGetApp().app_config->get_last_dir()) : wxString();
    wxFileDialog   dialog(wxGetApp().mainframe, wxString::Format(_L("Save the file from %s"), wxString::FromUTF8(extension)), dir, name,
                          wxString::Format("*.%s|*.%s", ext, ext), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    const fs::path path = fs::u8path(into_u8(dialog.GetPath()));
    std::ofstream  out(path, std::ios::binary | std::ios::trunc);
    out << answer.file_text;
    out.close();
    if (NotificationManager *nm = notifications())
        nm->push_notification(NotificationType::CustomNotification, NotificationManager::NotificationLevel::RegularNotificationLevel,
                              out ? (boost::format(_u8L("Saved %1%.")) % path.filename().u8string()).str()
                                  : (boost::format(_u8L("%1% could not be saved there.")) % path.filename().u8string()).str());
}

} // namespace

wxString outcome_text(Outcome outcome, const std::string &problem, const Limits &limits)
{
    switch (outcome) {
    case Outcome::Answered: return _L("Answered");
    case Outcome::TimedOut: return wxString::Format(_L("Ran past its time limit of %d s"), limits.timeout_ms / 1000);
    case Outcome::TooLarge: return wxString::Format(_L("Answer over %d KB"), int(limits.max_answer / 1024));
    case Outcome::NotJson: return _L("Answer was not valid JSON");
    case Outcome::Invalid: return wxString::Format(_L("Answer broke a rule: %s"), wxString::FromUTF8(problem));
    case Outcome::Failed:
        if (problem.rfind("exit code ", 0) == 0) return wxString::Format(_L("Stopped with exit code %s"), wxString::FromUTF8(problem.substr(10)));
        return wxString::Format(_L("Failed: %s"), wxString::FromUTF8(problem));
    case Outcome::NotStarted: return _L("Not started");
    case Outcome::Stopped: return _L("Stopped by you");
    }
    return wxString();
}

Runtime &Runtime::instance()
{
    static Runtime runtime;
    return runtime;
}

Runtime::~Runtime() { stop_worker(); }

void Runtime::shutdown()
{
    Runtime &r = instance();
    r.m_shut_down = true;
    r.stop_worker();
    r.m_listeners.clear();
}

fs::path Runtime::folder() const { return fs::u8path(data_dir()) / "extensions"; }

Host &Runtime::host()
{
    if (!m_host) {
        const fs::path data = fs::u8path(data_dir());
        m_host = std::make_unique<Host>(data / "extensions", data / "extension-state", fs::u8path(resources_dir()) / "extensions");
        m_host->load();
    }
    return *m_host;
}

void Runtime::reload()
{
    host().load();
    notify_changed();
}

// ---------------------------------------------------------------- hooks

void Runtime::models_imported(const std::vector<std::string> &files, const std::vector<std::size_t> &objects)
{
    if (m_shut_down || objects.empty()) return;
    std::vector<PlannedRun> runs = host().plan(Hook::ModelImported);
    if (runs.empty()) return;
    Plater *plater = wxGetApp().plater();
    if (!plater) return;
    const Model                  &model = plater->model();
    std::vector<ModelObjectFacts> arrived;
    for (std::size_t i : objects) {
        if (i >= model.objects.size() || arrived.size() >= 500) continue;
        const ModelObject *o = model.objects[i];
        ModelObjectFacts   f;
        f.name      = o->name;
        f.instances = int(o->instances.size());
        f.triangles = o->facets_count();
        if (!o->instances.empty()) {
            const Vec3d size = o->instance_bounding_box(0).size();
            f.size           = {{size.x(), size.y(), size.z()}};
        }
        arrived.push_back(std::move(f));
    }
    if (arrived.empty()) return;
    const int plate = plater->get_partplate_list().get_curr_plate_index() + 1;
    Job       job;
    job.hook    = Hook::ModelImported;
    job.runs    = std::move(runs);
    job.context = json{{"model", imported_section(arrived, plate, files)}};
    const std::string first = files.empty() ? arrived.front().name : fs::u8path(files.front()).filename().u8string();
    job.subject = files.size() > 1 ? (boost::format(_u8L("%1% and %2% more")) % first % (files.size() - 1)).str() : first;
    submit(std::move(job));
}

void Runtime::plate_sliced(PartPlate *plate, const Print *print)
{
    if (m_shut_down || !plate || !print) return;
    // One run per slice result: exporting an already sliced plate keeps its timestamp.
    const std::size_t stamp = print->step_state_with_timestamp(psGCodeExport).timestamp;
    const int         index = plate->get_index();
    const auto        seen  = m_sliced_stamp.find(index);
    if (seen != m_sliced_stamp.end() && seen->second == stamp) return;
    m_sliced_stamp[index] = stamp;

    std::vector<PlannedRun> runs = host().plan(Hook::PlateSliced);
    if (runs.empty()) return;
    const SliceFacts facts = facts_for(plate, print, plate->get_slice_result());
    Job              job;
    job.hook    = Hook::PlateSliced;
    job.runs    = std::move(runs);
    job.context = json{{"slice", slice_section(facts)}, {"printer", printer_section(printer_facts())}};
    job.subject = plate_subject(facts);
    submit(std::move(job));
}

void Runtime::export_finished(const std::string &path)
{
    if (m_shut_down) return;
    const fs::path p   = fs::u8path(path);
    std::string    ext = p.extension().u8string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    // Plain G-code only; a .3mf holds the G-code inside an archive.
    if (ext == ".gcode") m_pending_export = p;
}

void Runtime::process_completed(bool success, PartPlate *plate, const Print *print)
{
    if (m_pending_export.empty()) return;
    fs::path exported;
    std::swap(exported, m_pending_export);
    if (!success || m_shut_down) return;
    std::vector<PlannedRun> runs = host().plan(Hook::GcodeExported);
    if (runs.empty()) return;
    SliceFacts facts = facts_for(plate, print, plate ? plate->get_slice_result() : nullptr);
    facts.file       = exported.filename().u8string();
    Job job;
    job.hook    = Hook::GcodeExported;
    job.runs    = std::move(runs);
    job.context = json{{"slice", slice_section(facts)}, {"printer", printer_section(printer_facts())}};
    job.subject = facts.file;
    job.gcode   = exported;
    submit(std::move(job));
}

json Runtime::current_context(Hook hook, std::string &subject)
{
    json        context = json::object();
    int         plate_number = 0;
    const auto  objects      = model_facts(plate_number);
    context["model"]         = model_section(objects, plate_number);
    context["printer"]       = printer_section(printer_facts());
    Plater *plater = wxGetApp().plater();
    if (PartPlate *plate = plater ? plater->get_partplate_list().get_curr_plate() : nullptr) {
        if (plate->is_slice_result_valid()) {
            PrintBase   *base   = nullptr;
            GCodeResult *result = nullptr;
            int          index  = 0;
            plate->get_print(&base, &result, &index);
            context["slice"] = slice_section(facts_for(plate, dynamic_cast<const Print *>(base), result));
        }
    }
    int selected = 0;
    for (const ModelObjectFacts &o : objects)
        if (o.selected) ++selected;
    subject = selected > 0 ? (boost::format(_u8L("%1% selected of %2% objects")) % selected % objects.size()).str()
                           : (boost::format(_u8L("Plate %1%, nothing selected")) % plate_number).str();
    (void) hook;
    return context;
}

// ---------------------------------------------------------------- before a print is sent

std::vector<PlannedRun> Runtime::plan_before_send()
{
    if (m_shut_down) return {};
    return host().plan(Hook::PrintBeforeSend);
}

json Runtime::before_send_context(const SendFacts &send, std::string &subject)
{
    std::vector<SliceFacts> plates;
    if (Plater *plater = wxGetApp().plater()) {
        PartPlateList &list = plater->get_partplate_list();
        for (int i = 0; i < list.get_plate_count(); ++i) {
            if (!send.all_plates && i + 1 != send.plate) continue;
            PartPlate *plate = list.get_plate(i);
            if (!plate || !plate->is_slice_result_valid()) continue;
            PrintBase   *base   = nullptr;
            GCodeResult *result = nullptr;
            int          index  = 0;
            plate->get_print(&base, &result, &index);
            plates.push_back(facts_for(plate, dynamic_cast<const Print *>(base), result));
        }
    }
    json context = json::object();
    if (!plates.empty()) context["slice"] = slice_section(combine_plates(plates));
    context["printer"] = printer_section(printer_facts());
    context["send"]    = send_section(send);
    if (send.all_plates) subject = (boost::format(_u8L("All plates (%1%)")) % plates.size()).str();
    else if (plates.size() == 1) subject = plate_subject(plates.front());
    else subject = (boost::format(_u8L("Plate %1%")) % send.plate).str();
    return context;
}

std::shared_ptr<std::atomic<bool>> Runtime::run_before_send(std::vector<PlannedRun> runs, json context, std::string subject,
                                                            std::function<void(const RunResult &)> each, std::function<void()> done)
{
    if (m_shut_down || runs.empty()) return nullptr;
    Job job;
    job.hook    = Hook::PrintBeforeSend;
    job.runs    = std::move(runs);
    job.context = std::move(context);
    job.subject = std::move(subject);
    job.first   = true;
    job.each    = std::move(each);
    job.done    = std::move(done);
    job.cancel  = std::make_shared<std::atomic<bool>>(false);
    auto cancel = job.cancel;
    submit(std::move(job));
    return cancel;
}

void Runtime::stop(const std::shared_ptr<std::atomic<bool>> &cancel)
{
    if (!cancel) return;
    std::vector<std::function<void()>> dropped;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cancel->store(true);
        dropped = drop_queue(cancel);
    }
    for (auto &fn : dropped) post(std::move(fn));
    notify_changed();
}

std::vector<std::function<void()>> Runtime::drop_queue(const std::shared_ptr<std::atomic<bool>> &only)
{
    std::vector<std::function<void()>> dropped;
    for (auto it = m_queue.begin(); it != m_queue.end();) {
        if (only && it->cancel != only) {
            ++it;
            continue;
        }
        if (it->done) dropped.push_back(std::move(it->done));
        it = m_queue.erase(it);
    }
    return dropped;
}

// ---------------------------------------------------------------- commands

bool Runtime::run_action(const std::string &id, const std::string &action)
{
    if (m_shut_down) return false;
    std::optional<PlannedRun> run = host().plan_action(id, action);
    if (!run) return false;
    Job job;
    job.hook    = Hook::Command;
    job.runs    = {*run};
    job.context = current_context(Hook::Command, job.subject);
    submit(std::move(job));
    return true;
}

bool Runtime::try_sample(const std::string &id, Hook hook, const std::string &action)
{
    if (m_shut_down) return false;
    std::optional<PlannedRun> run = host().plan_sample(id, hook, action);
    if (!run) return false;
    Job job;
    job.hook    = hook;
    job.runs    = {*run};
    job.context = sample_context(hook);
    job.subject = _u8L("Sample request");
    job.sample  = true;
    submit(std::move(job));
    return true;
}

void Runtime::stop_all()
{
    std::vector<std::function<void()>> dropped;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        dropped = drop_queue();
        if (m_current_cancel) m_current_cancel->store(true);
    }
    for (auto &fn : dropped) post(std::move(fn));
    notify_changed();
}

Runtime::Activity Runtime::activity() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return Activity{m_running, m_running_hook, int(m_queue.size())};
}

// ---------------------------------------------------------------- worker

void Runtime::submit(Job job)
{
    if (m_shut_down || job.runs.empty()) return;
    if (!job.cancel) job.cancel = std::make_shared<std::atomic<bool>>(false);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping) return;
        if (job.first) m_queue.push_front(std::move(job));
        else m_queue.push_back(std::move(job));
    }
    ensure_worker();
    m_wake.notify_one();
    notify_changed();
}

void Runtime::ensure_worker()
{
    if (m_worker.joinable()) return;
    m_worker = std::thread([this]() { worker_loop(); });
}

void Runtime::post(std::function<void()> fn)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stopping || !wxTheApp) return;
    wxTheApp->CallAfter(std::move(fn));
}

void Runtime::worker_loop()
{
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_wake.wait(lock, [this]() { return m_stopping || !m_queue.empty(); });
            if (m_stopping) return;
            job = std::move(m_queue.front());
            m_queue.pop_front();
            m_current_cancel = job.cancel;
        }
        std::vector<CommentBlock> blocks;
        for (const PlannedRun &run : job.runs) {
            if (job.cancel->load()) break;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_stopping) return;
                m_running      = run.id;
                m_running_hook = run.hook == Hook::Command ? "command:" + run.action : info(run.hook).id;
            }
            post([]() { Runtime::instance().notify_changed(); });
            RunResult result = execute(run, job.context, job.subject, std::int64_t(std::time(nullptr)), job.cancel.get());
            if (result.outcome == Outcome::Answered && (!result.answer.gcode_prepend.empty() || !result.answer.gcode_append.empty()))
                blocks.push_back(CommentBlock{run.id, run.manifest.version, result.answer.gcode_prepend, result.answer.gcode_append});
            const bool sample = job.sample;
            post([result = std::move(result), sample, each = job.each]() { Runtime::instance().apply(result, sample, each); });
        }
        if (!job.gcode.empty() && !blocks.empty() && !job.cancel->load()) {
            std::string problem;
            const bool  ok   = add_comment_lines(job.gcode, blocks, problem);
            const auto  name = job.gcode.filename().u8string();
            const int   n    = int(blocks.size());
            if (!ok) BOOST_LOG_TRIVIAL(warning) << "extensions: comment lines not added to " << name << ": " << problem;
            post([name, n, ok, problem]() { Runtime::instance().apply_comment_lines(name, n, ok, problem); });
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_running.clear();
            m_running_hook.clear();
            m_current_cancel.reset();
        }
        if (job.done) post(std::move(job.done));
        post([]() { Runtime::instance().notify_changed(); });
    }
}

void Runtime::stop_worker()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
        m_queue.clear();
        if (m_current_cancel) m_current_cancel->store(true);
    }
    m_wake.notify_all();
    if (m_worker.joinable()) m_worker.join();
}

// ---------------------------------------------------------------- answers

void Runtime::apply(const RunResult &result, bool sample, const std::function<void(const RunResult &)> &each)
{
    if (m_shut_down || !m_host) return;
    m_host->record(result);
    if (each) {
        // print.before_send: the check window shows the answer and any failure.
        each(result);
        notify_changed();
        return;
    }
    const Installed     *x    = m_host->find(result.plan.id);
    const std::string    name = x ? x->manifest.name : result.plan.manifest.name;
    NotificationManager *nm   = notifications();
    if (result.outcome == Outcome::Answered) {
        std::vector<std::string> lines = result.answer.notes;
        if (sample) {
            const std::size_t start = result.answer.gcode_prepend.size(), end = result.answer.gcode_append.size();
            if (start + end > 0)
                lines.push_back((boost::format(_u8L("Sample answer: %1% start and %2% end comment lines, added to no file.")) % start % end).str());
            if (!result.answer.file_name.empty())
                lines.push_back((boost::format(_u8L("Sample answer: a file named %1%, not saved.")) % result.answer.file_name).str());
            if (!result.answer.checklist.empty()) {
                int pass = 0, warn = 0, fail = 0;
                for (const ChecklistRow &row : result.answer.checklist) (row.state == "pass" ? pass : row.state == "warn" ? warn : fail) += 1;
                lines.push_back((boost::format(_u8L("Sample answer: %1% checklist rows (%2% pass, %3% warn, %4% fail).")) %
                                 result.answer.checklist.size() % pass % warn % fail).str());
            }
            if (result.answer.block)
                lines.push_back((boost::format(_u8L("Sample answer: it would stop the print: %1%")) %
                                 (result.answer.block_reason.empty() ? _u8L("no reason given.") : result.answer.block_reason)).str());
            if (lines.empty()) lines.push_back(_u8L("Sample answer: valid, with nothing to show."));
        }
        if (!lines.empty() && nm)
            nm->push_notification(NotificationType::CustomNotification, NotificationManager::NotificationLevel::RegularNotificationLevel, name + ": " + join(lines, "\n"));
        if (!sample && result.plan.hook == Hook::Command && !result.answer.file_name.empty()) save_answer_file(name, result.answer);
    } else if (result.outcome != Outcome::Stopped && nm) {
        std::string text = (boost::format(_u8L("%1% did not answer: %2%.")) % name % into_u8(outcome_text(result.outcome, result.problem, result.plan.limits))).str();
        if (x && m_host->state(*x) == State::OffAfterFailures)
            text += " " + (boost::format(_u8L("It is off after %1% failed runs in a row.")) % m_host->limits().off_after_failures).str();
        const std::string id = result.plan.id;
        nm->push_notification(NotificationType::CustomNotification, NotificationManager::NotificationLevel::WarningNotificationLevel, text, _u8L("Open Extensions"),
                              [id](wxEvtHandler *) {
                                  Runtime::open_manager(id);
                                  return true;
                              });
    }
    notify_changed();
}

void Runtime::apply_comment_lines(const std::string &file, int extensions, bool ok, const std::string &problem)
{
    if (m_shut_down) return;
    if (NotificationManager *nm = notifications()) {
        if (ok)
            nm->push_notification(NotificationType::CustomNotification, NotificationManager::NotificationLevel::RegularNotificationLevel,
                                  (boost::format(_u8L("Comment lines from %1% extension(s) were added to %2%.")) % extensions % file).str());
        else
            nm->push_notification(NotificationType::CustomNotification, NotificationManager::NotificationLevel::WarningNotificationLevel,
                                  (boost::format(_u8L("Comment lines were not added to %1% (%2%). The exported file is unchanged.")) % file % problem).str());
    }
}

// ---------------------------------------------------------------- listeners and the palette

int Runtime::subscribe(std::function<void()> changed)
{
    const int token     = m_next_token++;
    m_listeners[token] = std::move(changed);
    return token;
}

void Runtime::unsubscribe(int token) { m_listeners.erase(token); }

void Runtime::notify_changed()
{
    if (m_shut_down) return;
    const auto listeners = m_listeners;
    for (const auto &[token, fn] : listeners)
        if (m_listeners.count(token) && fn) fn();
}

std::vector<Runtime::PaletteAction> Runtime::palette_actions()
{
    std::vector<PaletteAction> out;
    if (m_shut_down) return out;
    Host &h = host();
    for (const Installed &x : h.extensions()) {
        if (h.state(x) != State::On) continue;
        for (const Action &a : x.manifest.actions) out.push_back(PaletteAction{x.manifest.id, a.id, a.title, x.manifest.name});
    }
    return out;
}

void Runtime::open_manager(const std::string &select)
{
    MainFrame *frame = wxGetApp().mainframe;
    if (!frame) return;
    wxPanel *panel = frame->open_service("extensions", _L("Extensions"), [](wxWindow *parent) { return new ExtensionsPanel(parent); });
    if (auto *manager = dynamic_cast<ExtensionsPanel *>(panel); manager && !select.empty()) manager->select(select);
}

}}} // namespace Slic3r::GUI::Extensions
