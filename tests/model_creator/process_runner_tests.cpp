#define CATCH_CONFIG_RUNNER
#include "catch2/catch.hpp"
#include "slic3r/GUI/ModelCreator/ProcessRunner.hpp"

#include <windows.h>
#include <process.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

using namespace Slic3r::GUI::ModelCreator;

namespace {
std::filesystem::path self_path()
{
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    return {std::wstring(path, length)};
}

std::filesystem::path fixture_dir()
{
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    auto directory = std::filesystem::path(path) /
        (L"model-creator-process-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(directory);
    return directory;
}
}

int main(int argc, char *argv[])
{
    if (argc >= 3 && std::string(argv[1]) == "--grandchild") {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::ofstream(argv[2]) << "survived";
        return 0;
    }
    if (argc >= 3 && std::string(argv[1]) == "--spawn") {
        const auto self = self_path().wstring();
        const auto marker = std::filesystem::path(argv[2]).wstring();
        if (_wspawnl(_P_NOWAIT, self.c_str(), self.c_str(), L"--grandchild", marker.c_str(), nullptr) == -1)
            return 2;
        std::this_thread::sleep_for(std::chrono::seconds(20));
        return 0;
    }
    if (argc >= 2 && std::string(argv[1]) == "--probe") {
        wchar_t home[MAX_PATH];
        GetEnvironmentVariableW(L"HOME", home, MAX_PATH);
        const bool inherited = GetEnvironmentVariableW(L"MODEL_CREATOR_TEST_SECRET", nullptr, 0) > 0;
        std::cout << std::filesystem::path(home).string() << "\n" << inherited << "\n";
        return 0;
    }
    return Catch::Session().run(argc, argv);
}

TEST_CASE("Worker environment is isolated and does not inherit an unrelated secret")
{
    const auto directory = fixture_dir();
    const auto input = directory / "empty.txt", output = directory / "probe.txt";
    { std::ofstream stream(input); }
    SetEnvironmentVariableW(L"MODEL_CREATOR_TEST_SECRET", L"do-not-inherit");
    std::atomic_bool cancel{false};
    std::string error;
    const bool success = run_isolated_process(self_path(), {"--probe"}, input, output,
                                               directory, 5, cancel, ProcessRole::ClaudeCli, error);
    SetEnvironmentVariableW(L"MODEL_CREATOR_TEST_SECRET", nullptr);
    REQUIRE(success);
    std::ifstream stream(output);
    std::string home, inherited;
    std::getline(stream, home);
    std::getline(stream, inherited);
    REQUIRE(home.find("home") != std::string::npos);
    REQUIRE(inherited == "0");
}

TEST_CASE("Timeout kills the entire process tree")
{
    const auto directory = fixture_dir();
    const auto input = directory / "empty.txt", output = directory / "timeout.txt";
    const auto marker = directory / "timeout-marker.txt";
    std::filesystem::remove(marker);
    { std::ofstream stream(input); }
    std::atomic_bool cancel{false};
    std::string error;
    REQUIRE_FALSE(run_isolated_process(self_path(), {"--spawn", marker.string()}, input, output,
                                       directory, 1, cancel, ProcessRole::Renderer, error));
    REQUIRE(error == "Process timed out");
    std::this_thread::sleep_for(std::chrono::seconds(4));
    REQUIRE_FALSE(std::filesystem::exists(marker));
}

TEST_CASE("Cancellation kills the entire process tree")
{
    const auto directory = fixture_dir();
    const auto input = directory / "empty.txt", output = directory / "cancel.txt";
    const auto marker = directory / "cancel-marker.txt";
    std::filesystem::remove(marker);
    { std::ofstream stream(input); }
    std::atomic_bool cancel{false};
    std::thread trigger([&cancel] {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        cancel.store(true);
    });
    std::string error;
    const bool success = run_isolated_process(self_path(), {"--spawn", marker.string()}, input, output,
                                               directory, 10, cancel, ProcessRole::Renderer, error);
    trigger.join();
    REQUIRE_FALSE(success);
    REQUIRE(error == "Canceled");
    std::this_thread::sleep_for(std::chrono::seconds(4));
    REQUIRE_FALSE(std::filesystem::exists(marker));
}
