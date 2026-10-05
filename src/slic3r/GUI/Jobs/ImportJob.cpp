#include "ImportJob.hpp"
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <stdexcept>

namespace Slic3r::GUI {
namespace {
class ImportCanceled final : public std::exception {};

bool has_topology_metadata(const Model& model, const ModelObject& object, const ModelVolume& volume)
{
    return model.texture_mesh || object.is_cut() ||
           std::any_of(object.volumes.begin(), object.volumes.end(), [](const auto* other) { return !other->is_model_part(); }) ||
           volume.is_fdm_support_painted() || volume.is_fuzzy_skin_facets_painted() || volume.is_seam_painted() ||
           volume.is_mm_painted() || volume.emboss_shape.has_value() || volume.is_cut_connector();
}

}

ImportJob::ImportJob(std::vector<boost::filesystem::path> paths, ImportJobOptions options, Completion completion)
    : m_paths(std::move(paths)), m_options(std::move(options)), m_completion(std::move(completion)) {}

bool ImportJob::supports(const std::vector<boost::filesystem::path>& paths)
{
    return !paths.empty() && std::all_of(paths.begin(), paths.end(), [](const auto& path) {
        const std::string extension = boost::algorithm::to_lower_copy(path.extension().string());
        return extension == ".stl" || extension == ".oltp" || extension == ".obj" ||
               extension == ".step" || extension == ".stp" || extension == ".glb" ||
               extension == ".gltf" || extension == ".fbx" || extension == ".amf";
    });
}

void ImportJob::process(Ctl& ctl)
{
    auto check_cancel = [&ctl] { if (ctl.was_canceled()) throw ImportCanceled(); };
    try {
        for (size_t index = 0; index < m_paths.size(); ++index) {
            check_cancel();
            auto path = m_paths[index];
            path.make_preferred();
            PreparedImportFile file;
            auto report = [&](int percentage, const std::string& text) {
                ctl.update_status(static_cast<int>((index * 100 + std::clamp(percentage, 0, 100)) / m_paths.size()), text);
            };
            report(0, m_options.reading_text);
            const std::string extension = boost::algorithm::to_lower_copy(path.extension().string());
            if (extension == ".step" || extension == ".stp") {
                file.model = Model::read_from_step(path.string(), LoadStrategy::LoadModel,
                    [&](int, int current, int total, bool& cancel) {
                        cancel = ctl.was_canceled();
                        report(total > 0 ? current * 50 / total : 0, m_options.reading_text);
                    },
                    [&](int utf8) {
                        if (m_options.step_encoding_warning)
                            ctl.call_on_main_thread([&] { m_options.step_encoding_warning(utf8); }).get();
                    },
                    [&](Step& step, double& linear, double& angle, bool& split) {
                        check_cancel();
                        int answer = 1;
                        if (m_options.step_settings)
                            ctl.call_on_main_thread([&] { answer = m_options.step_settings(step, linear, angle, split); }).get();
                        if (answer < 0) throw ImportCanceled();
                        check_cancel();
                        return answer;
                    }, m_options.step_linear, m_options.step_angle, m_options.step_split,
                    [&](const std::vector<std::string>& names) {
                        if (m_options.step_shell_warning)
                            ctl.call_on_main_thread([&] { m_options.step_shell_warning(names); }).get();
                    });
            } else {
                bool gamma = extension == ".obj" && m_options.gamma_correct;
                file.model = Model::read_from_file(path.string(), nullptr, nullptr, LoadStrategy::LoadModel,
                    nullptr, nullptr, &gamma, nullptr, nullptr,
                    [&](int current, int total, bool& cancel, std::string& id, std::string& country,
                        std::string& region, std::string& name, std::string& maker_id) {
                        cancel = ctl.was_canceled();
                        file.designer_model_id = id; file.designer_country_code = country;
                        file.makerlab_region = region; file.makerlab_name = name; file.makerlab_id = maker_id;
                        report(total > 0 ? current * 50 / total : 0, m_options.reading_text);
                    });
                if (extension == ".amf") file.imperial_units = gamma;
            }
            check_cancel();
            // Prepare recovery hulls off-thread too. Model copies share the
            // immutable mesh/hull buffers; replacement below preserves them.
            for (auto* object : file.model.objects)
                for (auto* volume : object->volumes) {
                    check_cancel();
                    volume->calculate_convex_hull();
                }
            m_result.originals.emplace_back(file.model);
            size_t volume_count = 0, volume_index = 0;
            for (const auto* object : file.model.objects) volume_count += object->volumes.size();
            for (auto* object : file.model.objects) {
                for (auto* volume : object->volumes) {
                    check_cancel();
                    bool changed = false;
                    if (m_options.auto_simplify && should_simplify_mesh(volume->mesh().its.indices.size())) {
                        if (has_topology_metadata(file.model, *object, *volume)) {
                            ++m_result.skipped_volumes;
                        } else {
                            auto reduced = volume->mesh().its;
                            if (simplify_mesh(reduced, m_options.simplification, check_cancel, [&](int percent) {
                                report(50 + static_cast<int>((volume_index * 40 + percent * 40 / 100) / std::max<size_t>(1, volume_count)),
                                       m_options.simplifying_text);
                            })) {
                                // Keep the quadric engine's error-limited coordinates.
                                // Per-axis bounds rescaling would distort that result.
                                check_cancel();
                                volume->set_mesh(std::move(reduced));
                                volume->invalidate_convex_hull_2d();
                                changed = true;
                                ++m_result.simplified_volumes;
                            }
                        }
                    }
                    report(90, m_options.preparing_text);
                    // Convex hull computation is expensive for dense original
                    // geometry too. Do it before attaching to the live scene.
                    if (changed) volume->calculate_convex_hull();
                    check_cancel();
                    ++volume_index;
                }
                object->invalidate_bounding_box();
            }
            m_result.files.emplace_back(std::move(file));
            report(100, m_options.preparing_text);
        }
    } catch (const ImportCanceled&) {
        // Canceled batches never reach the completion callback, even when a
        // reader canceled before the worker's own flag was set.
        m_result.files.clear();
        m_result.originals.clear();
    }
}

void ImportJob::finalize(bool canceled, std::exception_ptr& exception)
{
    if (!canceled && !exception && m_result.files.size() == m_paths.size())
        m_completion(std::move(m_result));
}
} // namespace Slic3r::GUI
