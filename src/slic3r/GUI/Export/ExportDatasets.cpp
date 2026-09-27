#include "ExportDatasets.hpp"

#include "libslic3r/AppConfig.hpp"
#include "libslic3r/ExtrusionEntity.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Preset.hpp"
#include "libslic3r/ProjectHistoryManager.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace Slic3r::GUI::Export {

namespace {

// ISO-8601 with the recorded UTC offset, e.g. 2026-09-08T14:03:11+08:00.
std::string iso_timestamp(std::chrono::system_clock::time_point tp, int utc_offset_minutes)
{
    const std::time_t seconds = std::chrono::system_clock::to_time_t(tp) + static_cast<std::time_t>(utc_offset_minutes) * 60;
    std::tm            tm{};
#ifdef _WIN32
    gmtime_s(&tm, &seconds);
#else
    gmtime_r(&seconds, &tm);
#endif
    char buf[48];
    const int  abs_minutes = utc_offset_minutes < 0 ? -utc_offset_minutes : utc_offset_minutes;
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec, utc_offset_minutes < 0 ? '-' : '+', abs_minutes / 60, abs_minutes % 60);
    return buf;
}

const char *move_type_name(EMoveType t)
{
    switch (t) {
    case EMoveType::Noop: return "noop";
    case EMoveType::Retract: return "retract";
    case EMoveType::Unretract: return "unretract";
    case EMoveType::Seam: return "seam";
    case EMoveType::Tool_change: return "tool_change";
    case EMoveType::Color_change: return "color_change";
    case EMoveType::Pause_Print: return "pause_print";
    case EMoveType::Custom_GCode: return "custom_gcode";
    case EMoveType::Travel: return "travel";
    case EMoveType::Wipe: return "wipe";
    case EMoveType::Extrude: return "extrude";
    default: return "unknown";
    }
}

const char *custom_gcode_name(CustomGCode::Type t)
{
    switch (t) {
    case CustomGCode::ColorChange: return "color_change";
    case CustomGCode::PausePrint: return "pause_print";
    case CustomGCode::ToolChange: return "tool_change";
    case CustomGCode::Template: return "template";
    case CustomGCode::Custom: return "custom";
    default: return "unknown";
    }
}

const char *volume_type_name(ModelVolumeType t)
{
    switch (t) {
    case ModelVolumeType::MODEL_PART: return "part";
    case ModelVolumeType::NEGATIVE_VOLUME: return "negative_volume";
    case ModelVolumeType::PARAMETER_MODIFIER: return "modifier";
    case ModelVolumeType::SUPPORT_BLOCKER: return "support_blocker";
    case ModelVolumeType::SUPPORT_ENFORCER: return "support_enforcer";
    default: return "invalid";
    }
}

template<typename Map> Value index_map(const Map &m)
{
    Value obj = Value::make_object();
    for (const auto &kv : m) obj.set(std::to_string(kv.first), Value::from_number(kv.second));
    return obj;
}

} // namespace

Dataset project_history_dataset(const std::vector<ProjectHistoryVersion> &versions, const std::string &project_name)
{
    Dataset d;
    d.name           = "Project version history";
    d.schema_id      = "bambustudio.project-history";
    d.schema_version = 1;
    d.kind           = DatasetKind::Tabular;
    d.file_stem      = "project-history";
    d.columns        = {{"project", Value::Type::String},     {"commit", Value::Type::String},
                        {"message", Value::Type::String},     {"author_name", Value::Type::String},
                        {"author_email", Value::Type::String}, {"committed_at", Value::Type::String},
                        {"utc_offset_minutes", Value::Type::Integer}, {"snapshot_bytes", Value::Type::Integer}};
    for (const ProjectHistoryVersion &v : versions) {
        d.rows.push_back({Value::from_string(project_name), Value::from_string(v.commit_id), Value::from_string(v.message),
                          Value::from_string(v.author_name), Value::from_string(v.author_email),
                          Value::from_string(iso_timestamp(v.committed_at, v.utc_offset_minutes)),
                          Value::from_int(v.utc_offset_minutes), Value::from_int(static_cast<long long>(v.snapshot_size))});
    }
    return d;
}

Dataset app_config_dataset(const AppConfig &config)
{
    Dataset d;
    d.name           = "Preferences";
    d.schema_id      = "bambustudio.app-config";
    d.schema_version = 1;
    d.kind           = DatasetKind::Structured;
    d.file_stem      = "preferences";
    d.root           = Value::make_object();
    for (const auto &section : config.storage()) {
        Value sec = Value::make_object();
        for (const auto &kv : section.second) sec.set(kv.first, Value::from_string(kv.second));
        d.root.set(section.first.empty() ? std::string("app") : section.first, std::move(sec));
    }
    return d;
}

Dataset preset_dataset(const Preset &preset, const std::string &preset_type)
{
    Dataset d;
    d.name           = preset_type + " preset: " + preset.name;
    d.schema_id      = "bambustudio.preset";
    d.schema_version = 1;
    d.kind           = DatasetKind::Structured;
    d.file_stem      = preset_type + "-preset";
    d.root           = Value::make_object();
    d.root.set("name", Value::from_string(preset.name));
    d.root.set("type", Value::from_string(preset_type));
    d.root.set("file", Value::from_string(preset.file));
    d.root.set("inherits", Value::from_string(preset.inherits()));
    d.root.set("is_system", Value::from_bool(preset.is_system));
    d.root.set("is_default", Value::from_bool(preset.is_default));
    d.root.set("is_external", Value::from_bool(preset.is_external));
    d.root.set("is_visible", Value::from_bool(preset.is_visible));
    d.root.set("is_dirty", Value::from_bool(preset.is_dirty));
    d.root.set("setting_id", Value::from_string(preset.setting_id));
    d.root.set("filament_id", Value::from_string(preset.filament_id));
    d.root.set("base_id", Value::from_string(preset.base_id));
    Value options = Value::make_object();
    for (const std::string &key : preset.config.keys()) options.set(key, Value::from_string(preset.config.opt_serialize(key)));
    d.root.set("options", std::move(options));
    return d;
}

Dataset object_list_dataset(const Model &model)
{
    Dataset d;
    d.name           = "Object list";
    d.schema_id      = "bambustudio.object-list";
    d.schema_version = 1;
    d.kind           = DatasetKind::Tabular;
    d.file_stem      = "object-list";
    d.columns        = {{"index", Value::Type::Integer},   {"name", Value::Type::String},     {"printable", Value::Type::Bool},
                        {"instances", Value::Type::Integer}, {"parts", Value::Type::Integer}, {"modifiers", Value::Type::Integer},
                        {"negative_volumes", Value::Type::Integer}, {"support_volumes", Value::Type::Integer},
                        {"facets", Value::Type::Integer},  {"size_x_mm", Value::Type::Number}, {"size_y_mm", Value::Type::Number},
                        {"size_z_mm", Value::Type::Number}, {"volumes", Value::Type::String}};
    long long index = 0;
    for (const ModelObject *object : model.objects) {
        if (object == nullptr) continue;
        long long parts = 0, modifiers = 0, negatives = 0, supports = 0;
        std::string volume_names;
        for (const ModelVolume *volume : object->volumes) {
            if (volume == nullptr) continue;
            switch (volume->type()) {
            case ModelVolumeType::MODEL_PART: ++parts; break;
            case ModelVolumeType::PARAMETER_MODIFIER: ++modifiers; break;
            case ModelVolumeType::NEGATIVE_VOLUME: ++negatives; break;
            case ModelVolumeType::SUPPORT_BLOCKER:
            case ModelVolumeType::SUPPORT_ENFORCER: ++supports; break;
            default: break;
            }
            if (!volume_names.empty()) volume_names += "; ";
            volume_names += volume->name + " (" + volume_type_name(volume->type()) + ")";
        }
        Value sx = Value::null(), sy = Value::null(), sz = Value::null();
        if (!object->instances.empty()) {
            const BoundingBoxf3 bb = object->instance_bounding_box(0);
            if (bb.defined) {
                const Vec3d size = bb.size();
                sx = Value::from_number(size.x());
                sy = Value::from_number(size.y());
                sz = Value::from_number(size.z());
            }
        }
        d.rows.push_back({Value::from_int(index++), Value::from_string(object->name), Value::from_bool(object->printable),
                          Value::from_int(static_cast<long long>(object->instances.size())), Value::from_int(parts),
                          Value::from_int(modifiers), Value::from_int(negatives), Value::from_int(supports),
                          Value::from_int(static_cast<long long>(object->facets_count())), sx, sy, sz,
                          Value::from_string(volume_names)});
    }
    return d;
}

Dataset print_statistics_dataset(const GCodeProcessorResult &result, int plate_index, const std::string &plate_name)
{
    using Stats = PrintEstimatedStatistics;
    Dataset d;
    d.name           = "Print statistics";
    d.schema_id      = "bambustudio.print-statistics";
    d.schema_version = 1;
    d.kind           = DatasetKind::Structured;
    d.file_stem      = "print-statistics";
    d.root           = Value::make_object();
    d.root.set("plate_index", Value::from_int(plate_index));
    d.root.set("plate_name", Value::from_string(plate_name));
    d.root.set("gcode_file", Value::from_string(result.filename));

    const Stats &ps = result.print_statistics;
    Value        modes = Value::make_object();
    const char  *mode_names[] = {"normal", "stealth"};
    for (std::size_t i = 0; i < ps.modes.size() && i < 2; ++i) {
        const Stats::Mode &m = ps.modes[i];
        Value mode = Value::make_object();
        mode.set("time_seconds", Value::from_number(m.time));
        mode.set("prepare_time_seconds", Value::from_number(m.prepare_time));
        Value moves = Value::make_object();
        for (const auto &kv : m.moves_times) moves.set(move_type_name(kv.first), Value::from_number(kv.second));
        mode.set("moves_times_seconds", std::move(moves));
        Value roles = Value::make_object();
        for (const auto &kv : m.roles_times) roles.set(ExtrusionEntity::role_to_string(kv.first), Value::from_number(kv.second));
        mode.set("roles_times_seconds", std::move(roles));
        Value customs = Value::make_array();
        for (const auto &kv : m.custom_gcode_times) {
            Value c = Value::make_object();
            c.set("type", Value::from_string(custom_gcode_name(kv.first)));
            c.set("start_seconds", Value::from_number(kv.second.first));
            c.set("duration_seconds", Value::from_number(kv.second.second));
            customs.push(std::move(c));
        }
        mode.set("custom_gcode_times", std::move(customs));
        Value layers = Value::make_array();
        for (float t : m.layers_times) layers.push(Value::from_number(t));
        mode.set("layer_times_seconds", std::move(layers));
        modes.set(mode_names[i], std::move(mode));
    }
    d.root.set("modes", std::move(modes));

    d.root.set("model_volumes_per_extruder_mm3", index_map(ps.model_volumes_per_extruder));
    d.root.set("wipe_tower_volumes_per_extruder_mm3", index_map(ps.wipe_tower_volumes_per_extruder));
    d.root.set("support_volumes_per_extruder_mm3", index_map(ps.support_volumes_per_extruder));
    d.root.set("total_volumes_per_extruder_mm3", index_map(ps.total_volumes_per_extruder));
    d.root.set("flush_per_filament_mm3", index_map(ps.flush_per_filament));
    d.root.set("load_time_per_filament_seconds", index_map(ps.load_time_per_filament));
    d.root.set("unload_time_per_filament_seconds", index_map(ps.unload_time_per_filament));
    Value per_role = Value::make_object();
    for (const auto &kv : ps.used_filaments_per_role) {
        Value r = Value::make_object();
        r.set("length_mm", Value::from_number(kv.second.first));
        r.set("volume_mm3", Value::from_number(kv.second.second));
        per_role.set(ExtrusionEntity::role_to_string(kv.first), std::move(r));
    }
    d.root.set("used_filament_per_role", std::move(per_role));
    Value color_changes = Value::make_array();
    for (double v : ps.volumes_per_color_change) color_changes.push(Value::from_number(v));
    d.root.set("volumes_per_color_change_mm3", std::move(color_changes));
    d.root.set("total_filament_changes", Value::from_int(ps.total_filament_changes));
    d.root.set("total_flush_filament_changes", Value::from_int(ps.total_flush_filament_changes));

    Value filaments = Value::make_array();
    for (std::size_t i = 0; i < result.filament_diameters.size(); ++i) {
        Value f = Value::make_object();
        f.set("index", Value::from_int(static_cast<long long>(i)));
        f.set("diameter_mm", Value::from_number(result.filament_diameters[i]));
        if (i < result.filament_densities.size()) f.set("density_g_cm3", Value::from_number(result.filament_densities[i]));
        if (i < result.filament_costs.size()) f.set("cost_per_kg", Value::from_number(result.filament_costs[i]));
        filaments.push(std::move(f));
    }
    d.root.set("filaments", std::move(filaments));
    return d;
}

} // namespace Slic3r::GUI::Export
