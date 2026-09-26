#ifndef slic3r_TaskManager_hpp_
#define slic3r_TaskManager_hpp_

#include "DeviceManager.hpp"
#include "slic3r/Utils/NetworkAgent.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/trivial.hpp>
#include <atomic>
#include <memory>
#include <mutex>


namespace Slic3r { 

enum TaskState
{
    TS_PENDING = 0,
    TS_SENDING,
    TS_SEND_COMPLETED,
    TS_SEND_CANCELED,
    TS_SEND_FAILED,
    TS_PRINTING,
    /* queray in Machine Object: IDLE, PREPARE, RUNNING, PAUSE, FINISH, FAILED, SLICING */
    TS_PRINT_SUCCESS,
    TS_PRINT_FAILED,
    TS_REMOVED,
    TS_IDLE,
};

std::string get_task_state_enum_str(TaskState ts);

class TaskStateInfo
{
public:
    static int g_task_info_id;
    typedef std::function<void(TaskState state, int percent)> StateChangedFn;

    TaskStateInfo(const BBL::PrintParams param);

    TaskStateInfo() {
        task_info_id = ++TaskStateInfo::g_task_info_id;
    }

    TaskState state() const { return m_state->load(); }
    void set_state(TaskState ts) {
        BOOST_LOG_TRIVIAL(trace) << "TaskStateInfo set state = " << get_task_state_enum_str(ts);
        m_state->store(ts);
        update();
    }
    BBL::PrintParams get_params() { return m_params; }

    BBL::PrintParams& params() { return m_params; }

    std::string get_job_id(){return profile_id;}

    void update_sending_percent(int percent) {
        m_sending_percent->store(percent);
        update();
    }
    void set_sent_time(std::chrono::system_clock::time_point time) {
        sent_time = time;
        update();
    }
    void set_state_changed_fn(StateChangedFn fn) {
        {
            std::lock_guard<std::mutex> lock(*m_state_changed_mutex);
            m_state_changed_fn = std::move(fn);
        }
        update();
    }
    void set_cancel_fn(WasCancelledFn fn) {
        cancel_fn = fn;
    }

    void set_task_name(std::string name) { m_task_name = name; }
    void set_device_name(std::string name) { m_device_name = name; }
    void set_job_id(std::string job_id) { m_job_id = job_id; }

    void update() {
        StateChangedFn callback;
        {
            std::lock_guard<std::mutex> lock(*m_state_changed_mutex);
            callback = m_state_changed_fn;
        }
        if (callback) {
            callback(state(), m_sending_percent->load());
        }
    }

    void cancel();
    bool is_canceled() const { return m_cancel->load(); }

    std::string get_device_name() {return m_device_name;};
    std::string get_task_name() {return m_task_name;};
    std::string get_sent_time() {
        std::time_t time = std::chrono::system_clock::to_time_t(sent_time);
        std::tm* timeInfo = std::localtime(&time);

        std::stringstream ss;
        ss << std::put_time(timeInfo, "%Y-%m-%d %H:%M:%S");
        std::string str = ss.str();
        return str;
    };

    /* sending timelapse */
    std::chrono::system_clock::time_point sent_time;
    WasCancelledFn    cancel_fn;
    OnUpdateStatusFn  update_status_fn;
    OnWaitFn          wait_fn;
    std::string       thumbnail_url;
    std::string       start_time;
    std::string       end_time;
    std::string       profile_id;
    int               task_info_id;
private:
    // TaskStateInfo is copied for list views, so copies share this signal.
    std::shared_ptr<std::atomic_bool> m_cancel{std::make_shared<std::atomic_bool>(false)};
    std::shared_ptr<std::atomic<TaskState>> m_state{std::make_shared<std::atomic<TaskState>>(TaskState::TS_IDLE)};
    std::string       m_task_name;
    std::string       m_device_name;
    BBL::PrintParams  m_params;
    std::shared_ptr<std::atomic<int>> m_sending_percent{std::make_shared<std::atomic<int>>(0)};
    std::string       m_job_id;
    std::shared_ptr<std::mutex> m_state_changed_mutex{std::make_shared<std::mutex>()};
    StateChangedFn    m_state_changed_fn;
};

class TaskSettings
{
public:
    int sending_interval { 180 };    /* sending a job every 60 seconds */
    int max_sending_at_same_time { 1 };
};

class TaskGroup
{
public:
    std::vector<TaskStateInfo*> tasks;
    TaskSettings                settings;

    TaskGroup(TaskSettings s)
        : settings(s)
    {
    }

    void append(TaskStateInfo* task) {
        this->tasks.push_back(task);
    }

    bool need_schedule(std::chrono::system_clock::time_point last, TaskStateInfo* task);
};

class TaskManager 
{
public:
    static int MaxSendingAtSameTime;
    static int SendingInterval;
    TaskManager(NetworkAgent* agent);

    int start_print(const std::vector<BBL::PrintParams>& params, TaskSettings* settings = nullptr);

    static void set_max_send_at_same_time(int count);

    void start();
    void stop();

    std::map<int, TaskStateInfo*> get_local_task_list();

    /* curr_page is start with 0 */
    std::map<std::string, TaskStateInfo> get_task_list(int curr_page, int page_count, int& total);

    TaskState query_task_state(std::string dev_id);

private:
    int schedule(TaskStateInfo* task);

    boost::thread                 m_scedule_thread;

    std::vector<TaskGroup>        m_cache_map;
    std::mutex                    m_map_mutex;
    /* sending task list */
    std::vector<TaskStateInfo*>   m_scedule_list;
    std::vector<boost::thread*>   m_sending_thread_list;
    std::mutex                    m_scedule_mutex;
    std::mutex                    m_lan_transfer_mutex;
    bool                        m_started { false };
    NetworkAgent*               m_agent { nullptr };

    std::chrono::system_clock::time_point last_sent_timestamp;
};


wxDECLARE_EVENT(EVT_MULTI_SEND_LIMIT, wxCommandEvent);
} // namespace Slic3r

#endif
