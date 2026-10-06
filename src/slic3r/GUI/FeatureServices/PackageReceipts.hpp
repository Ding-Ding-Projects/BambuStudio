#pragma once
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace Slic3r::GUI::FeatureServices {
inline std::string parse_worker_hash(std::string_view bytes)
{
    if (bytes.size() == 65 && bytes.back() == '\n') bytes.remove_suffix(1);
    if (bytes.size() != 64) return {};
    for (const char ch : bytes)
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) return {};
    return std::string(bytes);
}
// This receipt binds staging bytes to the converter's executable verifier. It
// is not a publisher signature and is never described as one.
inline std::string converter_worker_hash(const std::filesystem::path& executable_directory)
{
    std::ifstream input(executable_directory / "converter-worker.sha256", std::ios::binary);
    if (!input) return {};
    std::array<char, 66> bytes{};
    input.read(bytes.data(), bytes.size());
    if (input.bad() || input.gcount() > 65) return {};
    return parse_worker_hash(std::string_view(bytes.data(), static_cast<std::size_t>(input.gcount())));
}
}
