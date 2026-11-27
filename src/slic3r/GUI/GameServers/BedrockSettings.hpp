#pragma once
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace GameServers {
enum class BedrockValue { Text, Bool, Integer, Decimal, Choice, Protected, Unknown };
struct BedrockField
{
    BedrockValue             kind = BedrockValue::Unknown;
    std::vector<std::string> choices;
    int                      low = 0, high = 2147483647;
};
const std::map<std::string,BedrockField> &bedrock_fields();
BedrockField bedrock_field(const std::string &key);
bool         bedrock_value_allowed(const BedrockField &field, const std::string &value);
struct BedrockSetting
{
    std::string key, value;
    std::size_t start = 0, end = 0;
    bool        duplicate = false;
};
std::vector<BedrockSetting> read_bedrock_settings(const std::string &bytes);
bool                        change_bedrock_setting(std::string &bytes, const std::string &key, const std::string &value);
}}} // namespace Slic3r::GUI::GameServers
