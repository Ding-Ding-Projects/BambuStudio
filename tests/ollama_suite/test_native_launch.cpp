#include "slic3r/GUI/OllamaSuite/NativeLaunchAdapter.hpp"
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
using namespace Slic3r::OllamaSuite;
namespace {
int checks = 0;
void check(bool value) { ++checks; if (!value) throw std::runtime_error("native launch assertion failed at check " + std::to_string(checks)); }
}
int main() {
    using namespace NativeLaunchDetail;
    check(supported_kind(LaunchKind::LlamaServer));
    check(!supported_kind(LaunchKind::OllamaChat));
    check(!supported_kind(static_cast<LaunchKind>(99)));
    check(profile_unavailable_reason(LaunchKind::LlamaServer).empty());
    check(profile_unavailable_reason(LaunchKind::OllamaChat).find("HTTP API") != std::string::npos);
    check(!profile_unavailable_reason(static_cast<LaunchKind>(99)).empty());
    check(quote_argument(L"") == L"\"\"");
    check(quote_argument(L"plain") == L"\"plain\"");
    check(quote_argument(L"C:\\model folder\\") == L"\"C:\\model folder\\\\\"");
    check(quote_argument(L"a\"b") == L"\"a\\\"b\"");
    const std::vector<std::wstring> arguments = {L"", L"plain", L"two words", L"C:\\model folder\\", L"embedded\"quote", L"a\\\\\"b", L"中文模型"};
#ifdef _WIN32
    for (const auto& argument : arguments) {
        const auto command = L"program.exe " + quote_argument(argument);
        int count = 0; LPWSTR* parsed = CommandLineToArgvW(command.c_str(), &count);
        check(parsed && count == 2);
        check(std::wstring(parsed[1]) == argument); LocalFree(parsed);
    }
#endif
    RedactedLaunchSnapshot snapshot{"profile-name", "llama3.2:3b", 4096, 8080, true};
    auto encoded = encode_snapshot(snapshot); auto decoded = decode_snapshot(encoded);
    check(decoded.has_value()); check(decoded->profile_id == snapshot.profile_id);
    check(decoded->model_tag == snapshot.model_tag); check(decoded->context_length == 4096);
    check(decoded->port == 8080); check(decoded->enabled); check(encode_snapshot(*decoded) == encoded);
    check(decode_snapshot(encode_snapshot({})).has_value());
    for (const auto& suffix : {"\n", "secret", "\0hidden"}) {
        if (suffix[0]) check(!decode_snapshot(encoded + suffix));
    }
    check(!decode_snapshot(encoded + std::string("\0hidden", 7)));
    check(!decode_snapshot(std::string(513, 'a')));
    check(!decode_snapshot("OLLP1\np\nm\n-1\n8080\n1\n"));
    check(!decode_snapshot("OLLP1\np\nm\n4096\n65536\n1\n"));
    check(!decode_snapshot("OLLP1\np\nm\n04096\n8080\n1\n"));
    check(!decode_snapshot("OLLP1\np\nm\n4096\n8080\n2\n"));
    check(!decode_snapshot("OLLP2\np\nm\n4096\n8080\n1\n"));
    check(!decode_snapshot("OLLP1\np\nm\n9999999\n8080\n1\n"));
    auto unsafe = snapshot; unsafe.profile_id = "../profile"; check(!safe_snapshot(unsafe));
    unsafe = snapshot; unsafe.model_tag = "tag\nsecret"; check(encode_snapshot(unsafe).empty());
    unsafe = snapshot; unsafe.context_length = 1048577; check(!safe_snapshot(unsafe));
    unsafe = snapshot; unsafe.port = 65536; check(!safe_snapshot(unsafe));
    // This executable never instantiates the native adapter, opens a picker,
    // writes launch state, creates a process, or contacts a model endpoint.
    std::cout << "native launch helpers: " << checks << " checks passed\n";
}
