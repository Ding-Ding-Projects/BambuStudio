// Runs the production pipe worker against a disposable, non-hosting script.
#include "src/slic3r/GUI/LanModelDrop/LocalHost.cpp"
#include "src/slic3r/GUI/LanModelDrop/LanModelDropModel.cpp"
#include <fstream>
#include <iostream>

using namespace Slic3r::GUI::LanModelDrop;
static void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
static const char *valid_template = R"({"schema":1,"ok":true,"address":"http://127.0.0.1:8833","lanUrl":"http://fixture.lan:8833/"})";
static std::string expand(std::string body)
{
    // Fixed RFC1918 test octets, never discovered from a real host.
    const int octets[] = {10, 20, 30, 40};
    std::string address;
    for (const int octet : octets) { if (!address.empty()) address += "."; address += std::to_string(octet); }
    const auto at = body.find("fixture.lan");
    if (at != std::string::npos) body.replace(at, 11, address);
    return body;
}
static void script(const std::string &body)
{
    std::filesystem::create_directories("lan-model-drop");
    std::ofstream output("lan-model-drop/host.ps1", std::ios::binary);
    output << "param([string]$Mode,[string]$PairingPipe)\n"
        "if ($Mode -ne 'Detached') { exit 2 }\n" << body;
}
static std::unique_ptr<LocalHost::Result> finished(LocalHost &host)
{
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (std::chrono::steady_clock::now() < until) {
        if (auto result = host.take_result()) return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    host.cancel();
    throw std::runtime_error("fixture transport timed out");
}
int main()
{
    try {
        const std::string valid = expand(valid_template);
        {
            std::ofstream config("pairing-config.json");
            config << R"({"app":{"lan_drop_address":"http://127.0.0.1:8833","lan_drop_enabled":true}})" << '\n' << "# checksum\n";
            config.close();
            require(persisted_station_matches("pairing-config.json", "http://127.0.0.1:8833", "true"), "saved config readback");
            require(!persisted_station_matches("pairing-config.json", "http://127.0.0.1:8834", "true"), "stale saved address rejected");
            require(!persisted_station_matches("missing-config.json", "http://127.0.0.1:8833", "true"), "missing saved config rejected");
            std::ofstream broken("pairing-config.json", std::ios::trunc); broken << "{"; broken.close();
            require(!persisted_station_matches("pairing-config.json", "http://127.0.0.1:8833", "true"), "corrupt saved config rejected");
        }
        {
            std::string bytes = valid;
            LocalHost::Result result;
            require(parse_result(bytes, result) && result.ok, "valid result accepted");
            require(result.address == "http://127.0.0.1:8833" && result.lan_url == expand("http://fixture.lan:8833/"), "address binding");
        }
        for (const auto &bad : {
            std::string(R"({"schema":1,"schema":1,"ok":false,"code":"bad"})"),
            std::string(R"({"schema":1,"ok":true,"address":"http://8.8.8.8:8833","lanUrl":"http://fixture.lan:8833/"})"),
            std::string(R"({"schema":1,"ok":true,"address":"http://127.0.0.1:8833","lanUrl":"http://fixture.lan:8834/"})"),
            std::string(R"({"schema":1,"ok":true,"address":"http://127.0.0.1:8833","lanUrl":"http://8.8.8.8:8833/"})"),
            std::string(R"({"schema":1,"ok":true,"address":"http://127.0.0.1:8833","lanUrl":"http://fixture.lan:8833/","unexpected":"field"})"),
            std::string(R"({"schema":1,"ok":false,"code":{"nested":1}})"), std::string("not json")}) {
            std::string bytes=expand(bad);
            LocalHost::Result result;
            require(!parse_result(bytes, result) && !result.ok, "invalid result rejected");
        }
        const std::string writer = "$rights=[IO.Pipes.PipeAccessRights]::Write -bor [IO.Pipes.PipeAccessRights]::ReadPermissions\n"
            "$p=[IO.Pipes.NamedPipeClientStream]::new('.', $PairingPipe, $rights, [IO.Pipes.PipeOptions]::None, "
            "[Security.Principal.TokenImpersonationLevel]::None, [IO.HandleInheritability]::None)\n"
            "$p.Connect(3000)\n$acl=$p.GetAccessControl()\n"
            "$rules=$acl.GetAccessRules($true,$true,[Security.Principal.SecurityIdentifier])\n"
            "$sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value\n"
            "if (-not $acl.AreAccessRulesProtected -or $rules.Count -ne 1 -or $rules[0].IdentityReference.Value -ne $sid "
            "-or $rules[0].IsInherited -or $rules[0].AccessControlType -ne 'Allow') { exit 3 }\n"
            "$w=[IO.StreamWriter]::new($p,[Text.UTF8Encoding]::new($false))\n$w.WriteLine('";
        script(writer + valid + "')\n$w.Dispose()\n$p.Dispose()\n");
        {
            LocalHost host;
            require(host.start(), "real worker starts");
            auto result=finished(host);
            require(result->ok && !host.running(), "real protected pipe succeeds");
        }
        script(writer + std::string(9000,'x') + "')\n$w.Dispose()\n$p.Dispose()\n");
        {
            LocalHost host;
            require(host.start(), "oversized worker starts");
            require(!finished(host)->ok, "oversized pipe result rejected");
        }
        script("Start-Sleep -Seconds 30\n");
        {
            LocalHost host, duplicate;
            require(host.start(), "cancellation worker starts");
            require(!duplicate.start(), "concurrent setup rejected");
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            const auto start=std::chrono::steady_clock::now();
            host.cancel();
            require(std::chrono::steady_clock::now()-start < std::chrono::seconds(3), "cancel joins promptly");
            require(!host.running(), "cancel stops worker");
        }
        std::cout << "PASS parser validation, real owner-only pipe, oversized reply, concurrent start and cancellation\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
