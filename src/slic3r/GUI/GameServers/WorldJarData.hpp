#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace Slic3r { namespace GUI { namespace GameServers {
struct WorldJarData {
    std::string source, version, error;
    std::filesystem::file_time_type modified{};
    std::uintmax_t size=0;
    int pack_format=0,minor_format=0;
    std::vector<std::string> biomes,structure_sets;
    nlohmann::json preset,noise,dimension;
    bool matches(const std::string &path) const;
};
struct VerticalWorld {
    int sea=63,min_y=-64,height=384;
    bool aquifers=true,ore_veins=true;
};
WorldJarData read_world_jar(const std::string &jar);
std::string write_vertical_pack(const WorldJarData &jar,const VerticalWorld &options,const std::filesystem::path &world);
}}}
