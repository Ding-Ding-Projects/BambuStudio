#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <filesystem>

namespace Slic3r { namespace GUI { namespace LanModelDrop {

// AppConfig::save() can return after an unsuccessful rename. Check the actual
// destination before treating automatic pairing (or its rollback) as durable.
bool persisted_station_matches(const std::filesystem::path &path, const std::string &address, const std::string &enabled);

// Owns one bounded local setup operation. No GUI pointer crosses into its worker.
// Destroying it cancels and joins the worker and closes only its own process tree.
class LocalHost
{
public:
    struct Result {
        bool ok = false;
        std::string code;
        std::string address;
        std::string lan_url;
    };
    LocalHost() = default;
    ~LocalHost();
    bool start();
    bool running() const { return m_running.load(); }
    std::unique_ptr<Result> take_result();
    void cancel();

private:
    void run();
    std::atomic<bool> m_cancel{false};
    std::atomic<bool> m_running{false};
    std::thread m_worker;
    std::mutex m_mutex;
    std::unique_ptr<Result> m_result;
};

}}}
