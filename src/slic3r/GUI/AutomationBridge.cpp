#include "AutomationBridge.hpp"
#include "GUI_App.hpp"
#include "Plater.hpp"
#include "GLCanvas3D.hpp"
#include "NotificationManager.hpp"
#include "Tab.hpp"
#include "PartPlate.hpp"
#include "DeviceCore/DevManager.h"
#include "DeviceCore/DevNozzleSystem.h"
#include "DeviceCore/DevMappingNozzle.h"
#include "TaskManager.hpp"
#include "BackgroundSlicingProcess.hpp"
#include "libslic3r/Model.hpp"
#include "nlohmann/json.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <map>
#include <mutex>
#include <thread>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#endif
namespace Slic3r { namespace GUI {
using Json = nlohmann::json;
namespace {
constexpr size_t max_message = 1024 * 1024;
Json failure(const std::string& id, const std::string& code, const std::string& message) {
    return {{"version",1},{"id",id},{"ok",false},{"error",{{"code",code},{"message",message}}}};
}
struct Rejected : std::runtime_error {
    std::string code;
    Rejected(std::string c, std::string message) : std::runtime_error(message), code(std::move(c)) {}
};
std::string required(const Json& args, const char* key) {
    if (!args.contains(key) || !args[key].is_string() || args[key].get<std::string>().empty())
        throw Rejected("invalid_arguments", std::string("Required nonempty string: ") + key);
    return args[key].get<std::string>();
}
}
struct AutomationBridge::State : std::enable_shared_from_this<AutomationBridge::State> {
    GUI_App& app;
    std::atomic<bool> stopping{false}, busy{false};
    std::thread worker;
    std::vector<std::filesystem::path> roots;
    std::map<std::string, Json> starts;
    struct Job { std::string kind; int task_id{-1}; int plate{-1}; size_t revision{0}; bool cancelled{false}; uint64_t generation{0}; uint64_t request_generation{0}; bool reused{false}; std::shared_ptr<void> staging_lease; };
    std::map<std::string, Job> jobs;
    std::mutex pending_mutex;
    std::vector<std::function<void()>> wake_pending;
    unsigned next_job{0};
#ifdef _WIN32
    std::mutex pipe_mutex;
    HANDLE pipe{INVALID_HANDLE_VALUE};
    std::vector<HANDLE> path_leases;
#endif
    explicit State(GUI_App& a) : app(a) {}
    std::filesystem::path path(const Json& args, bool output) {
        auto p = std::filesystem::u8path(required(args,"path"));
        if (!p.is_absolute() || roots.empty()) throw Rejected("path_denied","An absolute path inside BAMBU_AUTOMATION_ROOTS is required");
        p = p.lexically_normal();
#ifdef _WIN32
        // Reject junctions, symlinks, device paths and alternate data streams at every component.
        const auto native = p.native();
        if (native.rfind(L"\\\\",0)==0 || native.find(L':',3)!=std::wstring::npos)
            throw Rejected("path_denied","Device, network and alternate-stream paths are prohibited");
        std::filesystem::path walk;
        for (const auto& component : p) {
            walk /= component;
            if(!walk.has_root_directory()) continue;
            DWORD attr = GetFileAttributesW(walk.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT))
                throw Rejected("path_denied","Reparse points are prohibited");
            if(attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                HANDLE lease=CreateFileW(walk.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
                BY_HANDLE_FILE_INFORMATION info{};
                if(lease==INVALID_HANDLE_VALUE || !GetFileInformationByHandle(lease,&info) || (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)) {
                    if(lease!=INVALID_HANDLE_VALUE) CloseHandle(lease);
                    throw Rejected("path_denied","Cannot safely lock a path component");
                }
                path_leases.push_back(lease);
            }
        }
#endif
        auto resolved = std::filesystem::weakly_canonical(p);
        bool allowed = false;
        for (const auto& root : roots) {
            auto relative = resolved.lexically_relative(root);
            if (!relative.empty() && *relative.begin() != ".." && !relative.is_absolute()) allowed = true;
        }
        if (!allowed) throw Rejected("path_denied","Path is outside configured roots");
        if (output) {
            if (!std::filesystem::is_directory(p.parent_path())) throw Rejected("path_denied","Output parent must exist");
            // Replacement requires explicit consent and is published atomically.
            if (std::filesystem::exists(p) && !args.value("overwrite",false)) throw Rejected("already_exists","Explicit overwrite=true is required");
        } else if (!std::filesystem::is_regular_file(p)) throw Rejected("not_found","Input file does not exist");
        return p;
    }
    bool publish(const std::filesystem::path& target, bool overwrite, const std::function<bool(const std::filesystem::path&)>& write) {
#ifdef _WIN32
        auto temporary=target.parent_path()/(L".bambu-automation-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++next_job)+L".3mf");
        HANDLE reserved=CreateFileW(temporary.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(reserved==INVALID_HANDLE_VALUE) throw Rejected("save_failed","Cannot reserve temporary output");
        bool written=false;
        try {written=write(temporary);} catch(...) {CloseHandle(reserved); DeleteFileW(temporary.c_str()); throw;}
        CloseHandle(reserved);
        if(!written) {DeleteFileW(temporary.c_str()); return false;}
        if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH|(overwrite?MOVEFILE_REPLACE_EXISTING:0))) {
            DeleteFileW(temporary.c_str()); throw Rejected("save_failed","Atomic output publication failed");
        }
        return true;
#else
        throw Rejected("unsupported_platform","Named pipe automation requires Windows");
#endif
    }
    void record_print_intent(const std::string& request, const std::string& printer_id) {
#ifdef _WIN32
        // Intent is durable before dispatch. A restarted process never blindly replays it.
        const auto file=std::filesystem::u8path(Slic3r::data_dir())/"automation-print-intents.jsonl";
        std::filesystem::path walk;
        for(const auto& component:file.parent_path()) {
            walk/=component;if(!walk.has_root_directory()) continue;
            HANDLE lease=CreateFileW(walk.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
            BY_HANDLE_FILE_INFORMATION info{};
            if(lease==INVALID_HANDLE_VALUE || !GetFileInformationByHandle(lease,&info) || (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)) {
                if(lease!=INVALID_HANDLE_VALUE) CloseHandle(lease);
                throw Rejected("intent_unavailable","Print intent journal directory is unsafe");
            }
            path_leases.push_back(lease);
        }
        HANDLE token=nullptr;
        if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) throw Rejected("intent_unavailable","Cannot secure print intent journal");
        DWORD size=0; GetTokenInformation(token,TokenUser,nullptr,0,&size); std::vector<unsigned char> user(size);
        if(!GetTokenInformation(token,TokenUser,user.data(),size,&size)) {CloseHandle(token);throw Rejected("intent_unavailable","Cannot secure print intent journal");}
        LPWSTR sid=nullptr; BOOL converted=ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid,&sid); CloseHandle(token);
        if(!converted) throw Rejected("intent_unavailable","Cannot secure print intent journal");
        std::wstring acl=L"D:P(A;;GA;;;"+std::wstring(sid)+L")"; LocalFree(sid);
        PSECURITY_DESCRIPTOR sd=nullptr;
        if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&sd,nullptr)) throw Rejected("intent_unavailable","Cannot secure print intent journal");
        SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};
        HANDLE h=CreateFileW(file.c_str(),GENERIC_READ|GENERIC_WRITE|WRITE_DAC,0,&sa,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        BOOL present=FALSE, defaulted=FALSE; PACL dacl=nullptr;
        bool secured=h!=INVALID_HANDLE_VALUE && GetSecurityDescriptorDacl(sd,&present,&dacl,&defaulted) && present && SetSecurityInfo(h,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,nullptr,nullptr,dacl,nullptr)==ERROR_SUCCESS;
        LocalFree(sd);
        if(!secured) {if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);throw Rejected("intent_unavailable","Print intent journal is unavailable");}
        struct Close {HANDLE h;~Close(){CloseHandle(h);}} close{h};
        BY_HANDLE_FILE_INFORMATION info{}; LARGE_INTEGER length{};
        if(!GetFileInformationByHandle(h,&info) || (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) || !GetFileSizeEx(h,&length) || length.QuadPart>max_message)
            throw Rejected("intent_unavailable","Print intent journal is invalid or full");
        std::string contents(static_cast<size_t>(length.QuadPart),'\0'); DWORD count=0;
        if(!contents.empty() && (!ReadFile(h,&contents[0],static_cast<DWORD>(contents.size()),&count,nullptr) || count!=contents.size())) throw Rejected("intent_unavailable","Cannot read print intent journal");
        size_t pos=0; unsigned records=0;
        while(pos<contents.size()) {
            auto end=contents.find('\n',pos); if(end==std::string::npos) throw Rejected("intent_unavailable","Incomplete print intent journal; inspect printer state before repair");
            Json entry; try {entry=Json::parse(contents.substr(pos,end-pos));} catch(...) {throw Rejected("intent_unavailable","Invalid print intent journal");}
            if(!entry.is_object() || !entry.contains("requestId") || !entry["requestId"].is_string()) throw Rejected("intent_unavailable","Invalid print intent record");
            if(entry["requestId"]==request) throw Rejected("submission_unknown","This requestId was recorded by an earlier native process; inspect printer state and do not automatically replay");
            if(++records>=1024) throw Rejected("intent_full","Print intent journal is full; resolve old intents before reuse");
            pos=end+1;
        }
        auto line=Json({{"requestId",request},{"printerId",printer_id},{"intentOnly",true}}).dump()+"\n";
        if(contents.size()+line.size()>max_message || !WriteFile(h,line.data(),static_cast<DWORD>(line.size()),&count,nullptr) || count!=line.size() || !FlushFileBuffers(h)) throw Rejected("intent_unavailable","Cannot durably record print intent; no print was dispatched");
#else
        throw Rejected("unsupported_platform","Printer automation requires Windows");
#endif
    }
    MachineObject* printer(const Json& a) {
        auto id = required(a,"printerId");
        auto dm = app.getDeviceManager();
        auto machine = dm ? dm->get_my_machine(id) : nullptr;
        if (!machine) throw Rejected("printer_not_found","Explicit printer ID is not in the paired inventory");
        return machine;
    }
    Json machine_json(MachineObject* m) {
        return {{"printerId",m->get_dev_id()},{"name",m->get_dev_name()},{"connected",m->is_connected()},
                {"state",m->print_status},{"progress",m->mc_print_percent}};
    }
    int select_plate(Plater* plater, const Json& arguments) {
        int index = plater->get_partplate_list().get_curr_plate_index();
        if(arguments.contains("plateIndex") && !arguments["plateIndex"].is_null()) {
            if(!arguments["plateIndex"].is_number_integer()) throw Rejected("invalid_arguments","plateIndex must be a zero-based integer");
            const auto selected=arguments["plateIndex"].get<int64_t>();
            if(selected<0 || selected>=plater->get_partplate_list().get_plate_count()) throw Rejected("invalid_arguments","plateIndex is outside the project plate inventory");
            index=static_cast<int>(selected);
        }
        if(index!=plater->get_partplate_list().get_curr_plate_index()) {
            if(plater->is_background_process_slicing()) throw Rejected("busy","Cannot select a plate while slicing");
            if(plater->select_plate(index,false)!=0 || plater->get_partplate_list().get_curr_plate_index()!=index) throw Rejected("plate_selection_failed","Native plate selection failed");
        }
        return index;
    }
    Json execute(const std::string& op, const Json& a) {
#ifdef _WIN32
        struct LeaseRelease { State* state; ~LeaseRelease() {for(auto h:state->path_leases) CloseHandle(h);state->path_leases.clear();} } leases{this};
#endif
        auto p = app.plater();
        if (!p || p->is_loading_project()) throw Rejected("not_ready","Workspace is unavailable or loading");
        if (op == "capabilities") return {{"operations",Json::array({"capabilities","project_inspect","project_new","project_open","project_save","model_import","presets_list","settings_get","settings_update","slice_start","export_file","printer_list","printer_status","printer_start","printer_pause","printer_resume","printer_cancel","job_status","job_cancel"})},{"sliceWorkflow",{{"schemaVersion",1},{"operation","project_inspect"},{"diagnosticOnly",true},{"eventCapacity",16}}},{"maxMessageBytes",max_message},{"overwrite",true},{"enable","BAMBU_AUTOMATION=1"},{"requestIdScope","process result cache plus durable intent journal; earlier-process requests return submission_unknown"},{"plateIndexBase",0},{"exportFormats",Json::array({"stl","gcode.3mf"})},{"printerStartRequires",Json::array({"sliceJobId","printerId","requestId","path"})},{"printerStartRestrictions","one known reliable nozzle, one filament, external spool, matching printer model and diameter; AMS and dual nozzle are rejected"}};
        if (op == "project_inspect") {
            Json objects=Json::array(), plates=Json::array();
            for (size_t i=0;i<p->model().objects.size();++i) {
                auto o=p->model().objects[i];
                objects.push_back({{"index",i},{"name",o->name},{"instances",o->instances.size()},{"volumes",o->volumes.size()}});
            }
            for (auto plate:p->get_partplate_list().get_plate_list()) plates.push_back({{"index",plate->get_index()},{"sliceReady",plate->is_slice_result_ready_for_print()}});
            const auto observed = p->automation_slice_workflow();
            Json completions = Json::array(), continuations = Json::array();
            for (size_t i = 0; i < observed.completion_count; ++i) {
                const auto& e = observed.completions[i];
                completions.push_back({{"sequence",e.sequence},{"eventGeneration",e.event_generation},
                    {"currentGeneration",e.current_generation},{"status",e.status},
                    {"accepted",e.accepted},{"rejection",e.rejection}});
            }
            for (size_t i = 0; i < observed.continuation_count; ++i) {
                const auto& e = observed.continuations[i];
                continuations.push_back({{"sequence",e.sequence},{"requestGeneration",e.request_generation},
                    {"nativeGeneration",e.native_generation},{"plateIndex",e.plate_index},{"action",e.action}});
            }
            Json cancel = {{"visible",false}};
            auto* canvas = p->get_current_canvas3D();
            if (observed.enabled && canvas && canvas->get_wxglcanvas()->IsShownOnScreen()) {
                const auto target = p->get_notification_manager()->automation_slice_cancel_target(*canvas);
                const auto pixels = canvas->get_canvas_size();
                const auto client = canvas->get_wxglcanvas()->GetClientSize();
                if (target.visible && target.generation == observed.native_generation && target.x >= 0 && target.y >= 0 &&
                    target.x + target.width <= pixels.get_width() && target.y + target.height <= pixels.get_height() &&
                    pixels.get_width() > 0 && pixels.get_height() > 0) {
                    const auto origin = canvas->get_wxglcanvas()->ClientToScreen(wxPoint(0,0));
                    const double sx = double(client.x) / pixels.get_width(), sy = double(client.y) / pixels.get_height();
                    cancel = {{"visible",true},{"frame",target.frame},{"nativeGeneration",target.generation},{"ageMs",target.age_ms},
                        {"coordinateSpace","screen-pixels"},
                        {"rect",Json::array({origin.x + target.x * sx, origin.y + target.y * sy,
                            origin.x + (target.x + target.width) * sx, origin.y + (target.y + target.height) * sy})},
                        {"canvasRect",Json::array({origin.x,origin.y,origin.x + client.x,origin.y + client.y})}};
                }
            }
            Json workflow = {{"schemaVersion",1},{"enabled",observed.enabled},{"diagnosticOnly",true},
                {"eventCapacity",observed.event_capacity},{"requestGeneration",observed.request_generation},
                {"nativeGeneration",observed.native_generation},{"modelRevision",observed.model_revision},
                {"outcome",observed.outcome},{"cancellationRequested",observed.cancellation_requested},
                {"workerRunning",observed.worker_running},{"processingPlateIndex",observed.processing_plate_index},
                {"pending",{{"action",observed.pending_action},{"plateIndex",observed.pending_plate_index},
                    {"requestGeneration",observed.pending_request_generation},{"nativeGeneration",observed.pending_native_generation},
                    {"matchesCurrentPlate",observed.pending_matches_current_plate},
                    {"matchesProcessingPlate",observed.pending_matches_processing_plate}}},
                {"completionSequence",observed.completion_sequence},{"continuationSequence",observed.continuation_sequence},
                {"completionEvents",completions},{"continuationEvents",continuations},{"cancelTarget",cancel}};
            return {{"name",p->get_project_name().ToStdString()},{"dirty",p->is_project_dirty()},{"objects",objects},{"plates",plates},{"currentPlate",p->get_partplate_list().get_curr_plate_index()},{"slicing",p->is_background_process_slicing()},{"sliceWorkflow",workflow}};
        }
        if (op=="project_new" || op=="project_open") {
            if (p->is_project_dirty()) throw Rejected("unsaved_changes","Save the current project before replacing it");
            if (p->is_background_process_slicing()) throw Rejected("busy","Slicing must finish or be cancelled first");
            if (op=="project_new") { p->new_project(true,true); return {{"created",true}}; }
            auto file=path(a,false);
            if (file.extension()!=".3mf") throw Rejected("invalid_arguments","Project input must be .3mf");
            if (!p->load_snapshot_from(file.u8string())) throw Rejected("load_failed","Native project load failed");
            return {{"opened",true}};
        }
        if (op=="project_save" || op=="export_file") {
            if (p->is_background_process_slicing()) throw Rejected("busy","Slicing is active");
            auto file=path(a,true);
            if (op=="project_save" && file.extension()!=".3mf") throw Rejected("invalid_arguments","Project output must be .3mf");
            if (op=="project_save") {
                auto previous=p->project_history_identity();
                if (!publish(file,a.value("overwrite",false),[&](const std::filesystem::path& temp) {return p->export_3mf(boost::filesystem::path(temp.u8string()),SaveStrategy::Silence)>=0;})) throw Rejected("save_failed","Native project serialization failed");
                p->set_project_filename(wxString::FromUTF8(file.u8string().c_str()));
                p->reset_project_dirty_after_save();
                p->capture_saved_project_history(wxString::FromUTF8(file.u8string().c_str()),previous);
                return {{"saved",true},{"dirty",p->is_project_dirty()}};
            }
            int plate_index=select_plate(p,a);
            if(file.extension()==".stl") {
                if(!publish(file,a.value("overwrite",false),[&](const std::filesystem::path& temp) {return p->automation_export_plate_stl(temp,plate_index);})) throw Rejected("export_failed","Native STL export requires nonempty FFF geometry with no negative volumes");
                return {{"exported",true},{"format","stl"},{"plate",plate_index}};
            }
            if(file.extension()!=".3mf") throw Rejected("invalid_arguments","Supported export outputs are .stl and sliced .3mf");
            auto plate=p->get_partplate_list().get_curr_plate();
            if (!plate->is_slice_result_ready_for_export()) throw Rejected("not_ready","Current plate has no valid slice result");
            if (!publish(file,a.value("overwrite",false),[&](const std::filesystem::path& temp) {return p->export_3mf(boost::filesystem::path(temp.u8string()),SaveStrategy::Silence|SaveStrategy::WithGcode|SaveStrategy::SkipModel,plate->get_index())>=0;}))
                throw Rejected("export_failed","Native sliced archive export failed");
            return {{"exported",true},{"format","gcode.3mf"}};
        }
        if (op=="model_import") {
            if (p->is_background_process_slicing()) throw Rejected("busy","Slicing is active");
            auto file=path(a,false); auto ext=file.extension().string();
            if (ext!=".stl" && ext!=".obj" && ext!=".step" && ext!=".stp") throw Rejected("invalid_arguments","Supported model inputs: .stl, .obj, .step, .stp");
            auto indices=p->load_files(std::vector<std::string>{file.u8string()},LoadStrategy::LoadModel,false);
            if (indices.empty()) throw Rejected("import_failed","No model objects were imported");
            return {{"objectIndices",indices}};
        }
        if (op=="presets_list") {
            Json out=Json::object();
            if (!app.preset_bundle) throw Rejected("not_ready","Preset bundle unavailable");
            auto add=[&out](const char* key,const PresetCollection& c) { out[key]=Json::array(); for(const auto& preset:c.get_presets()) out[key].push_back(preset.name); };
            add("print",app.preset_bundle->prints); add("filament",app.preset_bundle->filaments); add("printer",app.preset_bundle->printers); return out;
        }
        if (op=="settings_get" || op=="settings_update") {
            // Limit automation to scalar print parameters, never scripts, network or account settings.
            const std::vector<std::string> keys={"layer_height","sparse_infill_density","wall_loops","top_shell_layers","bottom_shell_layers"};
            if (op=="settings_update") {
                if(p->is_background_process_slicing()) throw Rejected("busy","Slicing is active");
                if(!a.contains("values") || !a["values"].is_object()) throw Rejected("invalid_arguments","values must be an object");
                DynamicPrintConfig changes;
                for(auto it=a["values"].begin();it!=a["values"].end();++it) {
                    if(std::find(keys.begin(),keys.end(),it.key())==keys.end() || !it.value().is_number()) throw Rejected("invalid_arguments","Setting is outside the scalar allowlist");
                    double v=it.value().get<double>();
                    double upper=it.key()=="layer_height"?1.0:it.key()=="sparse_infill_density"?100.0:100.0;
                    if(!std::isfinite(v) || v<0 || v>upper || (it.key()=="layer_height" && v<0.01)) throw Rejected("invalid_arguments","Setting is outside supported bounds");
                    changes.set_deserialize_strict(it.key(),it.value().dump());
                }
                auto tab=app.get_tab(Preset::TYPE_PRINT);
                if(!tab || !app.preset_bundle) throw Rejected("not_ready","Print settings editor unavailable");
                tab->load_config(changes);
                p->on_config_change(app.preset_bundle->full_config());
            }
            auto config=app.preset_bundle?app.preset_bundle->full_config():*p->config();
            Json values=Json::object(); for(const auto& key:keys) if(auto option=config.option(key)) values[key]=option->serialize();
            return {{"values",values},{"scope","current project print parameters"}};
        }
        if (op=="slice_start") {
            if(p->is_background_process_slicing() || p->model().objects.empty()) throw Rejected("not_ready","Workspace is empty or already slicing");
            select_plate(p,a);
            if(jobs.size()>=128) throw Rejected("job_limit","Automation job inventory is full; restart only after resolving existing jobs");
            auto full_config=app.preset_bundle ? app.preset_bundle->full_config() : *p->config();
            auto post=full_config.option<ConfigOptionStrings>("post_process");
            if(post && std::any_of(post->values.begin(),post->values.end(),[](const std::string& command){return !command.empty();})) throw Rejected("unsafe_configuration","Remove post-processing commands before automated slicing");
            auto id="slice-"+std::to_string(++next_job); Job job; job.kind="slice"; job.plate=p->get_partplate_list().get_curr_plate_index(); job.revision=p->get_active_snapshot_time();
            auto before=p->background_process().automation_generation();
            p->reslice();
            job.generation=p->background_process().automation_generation(); job.request_generation=p->automation_slice_request_generation();
            job.reused=job.generation==before && p->get_partplate_list().get_curr_plate()->is_slice_result_ready_for_print();
            if(!job.reused && p->background_process().automation_outcome()!=1) throw Rejected("slice_not_started","Native slice request did not start processing");
            jobs[id]=job; return {{"jobId",id},{"state",job.reused?"completed":"running"},{"reused",job.reused},{"generation",job.generation},{"plate",job.plate},{"revision",job.revision}};
        }
        if (op=="printer_list") {
            Json list=Json::array(); auto dm=app.getDeviceManager(); if(dm) for(auto& item:dm->get_farm_machine_list()) if(item.second) list.push_back(machine_json(item.second)); return {{"printers",list}};
        }
        if (op=="printer_status") return machine_json(printer(a));
        if (op=="printer_pause" || op=="printer_resume" || op=="printer_cancel") {
            auto m=printer(a); if(!m->is_connected()) throw Rejected("not_ready","Printer is offline");
            if(op=="printer_resume" ? !m->is_in_printing_pause() : !m->is_in_printing()) throw Rejected("not_ready","Printer state does not permit this operation");
            int rc=op=="printer_pause"?m->command_task_pause():op=="printer_resume"?m->command_task_resume():m->command_task_abort();
            if(rc!=0) throw Rejected("printer_command_failed","Native printer command was not accepted");
            return {{"state","requested"},{"printerId",m->get_dev_id()}};
        }
        if (op=="printer_start") {
            auto request=required(a,"requestId");
            auto prior=starts.find(request); if(prior!=starts.end()) {
                if(prior->second["arguments"]!=a) throw Rejected("request_conflict","requestId was already used with different arguments");
                return prior->second["result"];
            }
            auto slice_id=required(a,"sliceJobId");
            auto slice=jobs.find(slice_id);
            if(slice==jobs.end() || slice->second.kind!="slice") throw Rejected("slice_job_not_found","sliceJobId must name a native slice job");
            const auto& sliced=slice->second;
            if(sliced.cancelled || sliced.plate!=p->get_partplate_list().get_curr_plate_index() || sliced.revision!=p->get_active_snapshot_time() || sliced.generation!=p->background_process().automation_generation() || sliced.request_generation!=p->automation_slice_request_generation()) throw Rejected("stale_job","Referenced slice job no longer matches the active workspace");
            if(a.contains("plateIndex") && !a["plateIndex"].is_null() && (!a["plateIndex"].is_number_integer() || a["plateIndex"]!=sliced.plate)) throw Rejected("invalid_arguments","plateIndex must match the referenced completed slice job");
            if((!sliced.reused && p->background_process().automation_outcome()!=2) || !p->get_partplate_list().get_curr_plate()->is_slice_result_ready_for_print()) throw Rejected("slice_not_completed","Referenced slice job must be successfully completed with a valid print result");
            if(starts.size()>=64 || jobs.size()>=128 || request.size()>128) throw Rejected("job_limit","Print request inventory is full or requestId exceeds 128 bytes");
            auto m=printer(a); auto plate=p->get_partplate_list().get_curr_plate();
            if(!m->is_connected() || m->is_in_printing() || (m->print_status!="IDLE" && m->print_status!="FINISH") || !plate->is_slice_result_ready_for_print()) throw Rejected("not_ready","Printer must be online and idle and the active plate sliced");
            // Mapping must be prepared by the same native UI flow that validates nozzle and AMS compatibility.
            if(!a.contains("nozzleMapping") || !a["nozzleMapping"].is_object() || !a.contains("amsMapping") || !a["amsMapping"].is_array()) throw Rejected("invalid_arguments","nozzleMapping object and amsMapping integer array are required");
            int plate_idx=plate->get_index();
            auto nozzles=m->GetNozzleSystem();
            if(!nozzles || nozzles->GetExtNozzleCount()!=1 || nozzles->HasUnknownNozzles() || nozzles->HasUnreliableNozzles())
                throw Rejected("unsupported_mapping","Automation currently requires one known, reliable installed nozzle");
            auto used=plate->get_extruders(true);
            if(used.size()!=1 || a.value("useAms",false) || a["amsMapping"]!=Json::array({-1}) || !a["nozzleMapping"].empty())
                throw Rejected("unsupported_mapping","Single-nozzle external-spool printing requires one used filament, useAms=false, amsMapping=[-1], nozzleMapping={}");
            auto full=app.preset_bundle?app.preset_bundle->full_config():*p->config();
            auto model=full.option("printer_model");
            auto diameter=full.option<ConfigOptionFloatsNullable>("nozzle_diameter");
            auto nozzle=nozzles->GetExtNozzles().begin()->second;
            if(!model || model->serialize()!=m->printer_type || !diameter || diameter->values.size()!=1 || std::abs(diameter->values[0]-nozzle.GetNozzleDiameter())>0.001)
                throw Rejected("printer_mismatch","Sliced printer model and nozzle diameter must match the target printer");
            // Existing farm scheduler owns SDK transfer serialization and cancellation.
            auto manager=app.getTaskManager(); if(!manager || !app.getAgent()) throw Rejected("not_ready","Network print scheduler unavailable");
            record_print_intent(request,m->get_dev_id());
            if(a.value("overwrite",false)) throw Rejected("invalid_arguments","Printer staging path must be new");
            auto file=path(a,true);
            if(file.extension()!=".3mf") throw Rejected("invalid_arguments","path must name a new .3mf staging archive");
            if(!publish(file,false,[&](const std::filesystem::path& temp) {return p->export_3mf(boost::filesystem::path(temp.u8string()),SaveStrategy::Silence|SaveStrategy::WithGcode|SaveStrategy::SkipModel,plate_idx)>=0;}))
                throw Rejected("export_failed","Native sliced archive preparation failed");
            BBL::PrintParams params{};
            params.dev_id=m->get_dev_id(); params.dev_name=m->get_dev_name(); params.dev_ip=m->get_dev_ip();
            params.connection_type=m->connection_type(); params.password=m->get_access_code(); params.username="bblp";
            if(params.connection_type=="lan" && (params.dev_ip.empty() || params.password.empty())) throw Rejected("not_ready","LAN printer must be paired with an address and access credential");
            params.filename=file.u8string(); params.config_filename=params.filename; params.plate_index=plate_idx+1; params.project_name=p->get_project_name().ToStdString(); params.task_name=params.project_name;
            params.ams_mapping=a["amsMapping"].dump(); params.nozzle_mapping=a["nozzleMapping"].dump();
            params.task_use_ams=a.value("useAms",false); params.task_bed_leveling=a.value("bedLeveling",true);
            params.use_ssl_for_ftp=true; params.use_ssl_for_mqtt=true; params.print_type="from_plater";
            TaskSettings settings; settings.max_sending_at_same_time=1; settings.sending_interval=0;

#ifdef _WIN32
            HANDLE staged=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
            if(staged==INVALID_HANDLE_VALUE) throw Rejected("print_start_failed","Cannot lock native print staging archive");
            std::shared_ptr<void> staging_lease(staged,[](void* handle){CloseHandle(handle);});
#else
            std::shared_ptr<void> staging_lease;
#endif
            int rc=manager->start_print({params},&settings); if(rc!=0) throw Rejected("print_start_failed","Native scheduler rejected print submission");
            auto id="print-"+std::to_string(++next_job); Job job; job.kind="print"; job.staging_lease=staging_lease;
            for(auto& item:manager->get_local_task_list()) if(item.second && item.second->get_params().filename==params.filename) {job.task_id=item.first;break;}
            jobs[id]=job; Json result={{"jobId",id},{"state","queued"},{"printerId",m->get_dev_id()}};
            starts[request]={{"arguments",a},{"result",result}}; return result;
        }
        if(op=="job_status" || op=="job_cancel") {
            auto id=required(a,"jobId"); auto it=jobs.find(id); if(it==jobs.end()) throw Rejected("job_not_found","Unknown automation job");
            auto& job=it->second;
            if(job.kind=="slice") {
                if(job.cancelled) return {{"state","cancelled"},{"completionVerified",true}};
                if(job.plate!=p->get_partplate_list().get_curr_plate_index() || job.revision!=p->get_active_snapshot_time() || job.generation!=p->background_process().automation_generation() || job.request_generation!=p->automation_slice_request_generation()) throw Rejected("stale_job","Workspace, active plate or slicing generation changed since submission");
                int outcome=p->background_process().automation_outcome();
                if(op=="job_cancel" && !job.reused && outcome==1) {
                    p->background_process().stop(); job.cancelled=true; return {{"state","cancelled"},{"completionVerified",true}};
                }
                bool ready=p->get_partplate_list().get_curr_plate()->is_slice_result_ready_for_print();
                return {{"state",job.reused || (outcome==2 && ready)?"completed":outcome==3?"failed":outcome==4?"cancelled":outcome==1 || (outcome==2 && !ready)?"running":"not_ready"},{"completionVerified",job.reused || outcome==2 || outcome==3 || outcome==4},{"generation",job.generation},{"plate",job.plate},{"revision",job.revision}};
            }
            auto manager=app.getTaskManager(); if(!manager) throw Rejected("not_ready","Print scheduler unavailable");
            if(job.task_id<0) throw Rejected("submission_unknown","Print was submitted but scheduler task identity is unavailable; inspect printer state and do not resubmit");
            auto list=manager->get_local_task_list(); auto task=list.find(job.task_id);
            if(task==list.end()) return {{"taskId",job.task_id},{"state",job.cancelled?"cancel_requested":"unavailable"}};
            if(op=="job_cancel") {task->second->cancel();job.cancelled=true;}
            return {{"taskId",job.task_id},{"state",get_task_state_enum_str(task->second->state())},{"cancelRequested",job.cancelled}};
        }
        throw Rejected("unknown_operation","Operation is not supported");
    }
    Json dispatch(const Json& request) {
        std::string id=request.is_object() && request.contains("id") && request["id"].is_string()?request["id"].get<std::string>():std::string();
        if(!request.is_object() || request.value("version",0)!=1 || id.empty() || id.size()>128 || !request.contains("operation") || !request["operation"].is_string() || !request.contains("arguments") || !request["arguments"].is_object()) return failure(id,"invalid_request","Expected version 1, string id/operation and object arguments");
        if(busy.exchange(true)) return failure(id,"busy","A native operation is still executing");
        struct Pending { std::mutex mutex; std::condition_variable cv; std::atomic<int> phase{0}; Json response; };
        auto pending=std::make_shared<Pending>(); auto self=shared_from_this();
        {std::lock_guard<std::mutex> lock(pending_mutex); wake_pending.clear(); wake_pending.push_back([pending]{pending->cv.notify_all();});}
        app.CallAfter([self,pending,request,id] {
            int queued=0;
            if(!pending->phase.compare_exchange_strong(queued,1)) return;
            if(self->stopping) {self->busy=false; pending->phase=2; pending->cv.notify_all(); return;}
            Json reply;
            try { reply={{"version",1},{"id",id},{"ok",true},{"result",self->execute(request["operation"],request["arguments"])}}; }
            catch(const Rejected& e) { reply=failure(id,e.code,e.what()); }
            catch(...) { reply=failure(id,"native_error","Native operation failed"); }
            {std::lock_guard<std::mutex> lock(pending->mutex); pending->response=std::move(reply); pending->phase=2;}
            self->busy=false; pending->cv.notify_all();
        });
        std::unique_lock<std::mutex> lock(pending->mutex);
        if(!pending->cv.wait_for(lock,std::chrono::seconds(30),[&]{return pending->phase==2 || stopping;})) {
            int queued=0; bool expired=pending->phase.compare_exchange_strong(queued,3);
            if(expired) busy=false;
            return failure(id,expired?"queue_timeout":"operation_in_progress",expired?"Queued operation expired without execution":"Native operation began and is still executing; inspect state before retrying");
        }
        return stopping?failure(id,"shutting_down","Application is closing"):pending->response;
    }
#ifdef _WIN32
    bool io(HANDLE h, bool write, void* data, DWORD bytes, DWORD& count, DWORD timeout_ms=30000) {
        OVERLAPPED ov{}; ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr); if(!ov.hEvent) return false;
        BOOL ok=write?WriteFile(h,data,bytes,&count,&ov):ReadFile(h,data,bytes,&count,&ov);
        if(!ok && GetLastError()==ERROR_IO_PENDING) {
            DWORD waited=WaitForSingleObject(ov.hEvent,timeout_ms);
            if(waited!=WAIT_OBJECT_0 || stopping) {CancelIoEx(h,&ov); WaitForSingleObject(ov.hEvent,INFINITE); ok=FALSE;}
            else ok=GetOverlappedResult(h,&ov,&count,FALSE);
        }
        CloseHandle(ov.hEvent); return ok && !stopping;
    }
    void serve() {
        HANDLE token=nullptr; if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) return;
        DWORD size=0; GetTokenInformation(token,TokenUser,nullptr,0,&size); std::vector<unsigned char> buf(size);
        if(!GetTokenInformation(token,TokenUser,buf.data(),size,&size)) {CloseHandle(token);return;}
        LPWSTR sid=nullptr; if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buf.data())->User.Sid,&sid)) {CloseHandle(token);return;}
        std::wstring acl=L"D:P(A;;GA;;;"+std::wstring(sid)+L")"; LocalFree(sid); CloseHandle(token);
        PSECURITY_DESCRIPTOR sd=nullptr; if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&sd,nullptr)) return;
        SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};
        auto name=L"\\\\.\\pipe\\BambuStudio.Automation.v1."+std::to_wstring(GetCurrentProcessId());
        while(!stopping) {
            HANDLE h=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,65536,65536,30000,&sa);
            if(h==INVALID_HANDLE_VALUE) break;
            {std::lock_guard<std::mutex> lock(pipe_mutex); pipe=h;}
            OVERLAPPED ov{}; ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
            BOOL connected=ConnectNamedPipe(h,&ov); DWORD error=connected?ERROR_SUCCESS:GetLastError();
            if(error==ERROR_IO_PENDING) { while(!stopping && WaitForSingleObject(ov.hEvent,100)!=WAIT_OBJECT_0) {} DWORD count=0; connected=!stopping && GetOverlappedResult(h,&ov,&count,FALSE); }
            else connected=connected || error==ERROR_PIPE_CONNECTED;
            if(!connected) {CancelIoEx(h,&ov); if(error==ERROR_IO_PENDING) WaitForSingleObject(ov.hEvent,INFINITE);}
            CloseHandle(ov.hEvent);
            std::string line;
            auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
            while(connected && !stopping) {
                auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
                if(remaining<=0) break;
                char buffer[8192]; DWORD count=0;
                if(!io(h,false,buffer,sizeof(buffer),count,static_cast<DWORD>(remaining)) || count==0) break;
                for(DWORD i=0;i<count && connected && !stopping;++i) {
                    char c=buffer[i];
                    if(c!='\n') {line+=c;if(line.size()>max_message) {connected=FALSE;break;}continue;}
                    Json reply;
                    try { reply=dispatch(Json::parse(line)); } catch(...) { reply=failure("","invalid_json","Invalid UTF-8 JSON request"); }
                    line.clear(); auto output=reply.dump()+"\n";
                    if(output.size()>max_message) output=failure(reply.value("id",std::string()),"response_too_large","Response exceeds protocol limit").dump()+"\n";
                    size_t offset=0;
                    while(offset<output.size()) {DWORD written=0;if(!io(h,true,&output[offset],static_cast<DWORD>(output.size()-offset),written) || written==0) {connected=FALSE;break;} offset+=written;}
                    deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
                }
            }
            {std::lock_guard<std::mutex> lock(pipe_mutex); CancelIoEx(h,nullptr); DisconnectNamedPipe(h); CloseHandle(h); pipe=INVALID_HANDLE_VALUE;}
        }
        LocalFree(sd);
    }
#endif
};
AutomationBridge::AutomationBridge(GUI_App& app) : m_state(std::make_shared<State>(app)) {}
AutomationBridge::~AutomationBridge() { stop(); }
void AutomationBridge::start() {
#ifdef _WIN32
    const char* enabled=std::getenv("BAMBU_AUTOMATION"); if(!enabled || std::string(enabled)!="1") return;
    const char* roots=std::getenv("BAMBU_AUTOMATION_ROOTS"); if(roots) {
        std::string list(roots); size_t pos=0;
        while(pos<=list.size()) {auto end=list.find(';',pos); auto item=list.substr(pos,end==std::string::npos?end:end-pos); if(!item.empty()) {auto p=std::filesystem::u8path(item); if(p.is_absolute() && std::filesystem::is_directory(p)) m_state->roots.push_back(std::filesystem::canonical(p));} if(end==std::string::npos) break; pos=end+1;}
    }
    auto state=m_state; state->worker=std::thread([state]{state->serve();});
#endif
}
void AutomationBridge::stop() {
    if(!m_state) return; m_state->stopping=true;
    {std::lock_guard<std::mutex> lock(m_state->pending_mutex); for(auto& wake:m_state->wake_pending) wake();}
#ifdef _WIN32
    {std::lock_guard<std::mutex> lock(m_state->pipe_mutex); if(m_state->pipe!=INVALID_HANDLE_VALUE) CancelIoEx(m_state->pipe,nullptr);}
#endif
    if(m_state->worker.joinable()) m_state->worker.join();
}
}}
