#pragma once
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <optional>
#include <string>

namespace Slic3r { namespace GUI { namespace GameServers {
struct ServerMetrics {
    using Clock = std::chrono::steady_clock;
    struct Point { Clock::time_point when; std::uint64_t bytes; };
    std::deque<Point> memory;
    std::optional<double> tps;
    Clock::time_point last_tps{}, requested{};

    void sample(Clock::time_point now, std::uint64_t bytes)
    {
        while (!memory.empty() && now-memory.front().when>std::chrono::minutes(30)) memory.pop_front();
        if (bytes && (memory.empty() || now-memory.back().when >= std::chrono::seconds(5))) memory.push_back({now,bytes});
        while (memory.size()>361) memory.pop_front();
    }
};

inline std::optional<double> paper_one_minute_tps(const std::string &source)
{
    // Chat lines and unrelated numbers are never interpreted as server measurements.
    std::string text;
    for (std::size_t i=0;i<source.size();++i) {
        if (source.compare(i,2,"\xc2\xa7")==0 && i+2<source.size()) {i+=2;continue;}
        if (source.compare(i,2,"\x1b[")==0) {
            auto end=source.find('m',i+2);
            if (end==std::string::npos || end-i>24) return {};
            i=end;continue;
        }
        text+=source[i];
    }
    constexpr const char prefix[]="TPS from last 1m, 5m, 15m: ";
    if (text.compare(0,sizeof(prefix)-1,prefix)!=0) return {};
    const auto start=text.find_first_not_of(" *",sizeof(prefix)-1);
    if (start==std::string::npos || !std::isdigit(static_cast<unsigned char>(text[start]))) return {};
    char *end=nullptr;
    const auto value=std::strtod(text.c_str()+start,&end);
    if (*end!=',' || !std::isfinite(value) || value<=0 || value>20.1) return {};
    return value;
}
}}}
