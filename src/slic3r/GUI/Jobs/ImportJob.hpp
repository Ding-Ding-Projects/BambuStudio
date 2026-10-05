#ifndef SLIC3R_IMPORT_JOB_HPP
#define SLIC3R_IMPORT_JOB_HPP

#include "JobNew.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/MeshSimplification.hpp"
#include <boost/filesystem/path.hpp>

namespace Slic3r::GUI {

// All inputs are captured on the UI thread. No live scene or application
// configuration is accessed by process(). Dialog callbacks are explicitly
// dispatched through Ctl::call_on_main_thread by the reader.
struct ImportJobOptions {
    bool auto_simplify = true;
    bool gamma_correct = false;
    double step_linear = 0.003;
    double step_angle = 0.5;
    bool step_split = false;
    MeshSimplificationOptions simplification;
    std::function<int(Step&, double&, double&, bool&)> step_settings;
    std::function<void(int)> step_encoding_warning;
    std::function<void(const std::vector<std::string>&)> step_shell_warning;
    std::string reading_text;
    std::string simplifying_text;
    std::string preparing_text;
};

struct PreparedImportFile {
    Model model;
    bool imperial_units = false;
    std::string designer_model_id, designer_country_code;
    std::string makerlab_region, makerlab_name, makerlab_id;
};

struct PreparedImport {
    std::vector<PreparedImportFile> files;
    // Model copies share immutable mesh storage until replacement. Originals
    // remain available for recovery without writing or changing input files.
    std::vector<Model> originals;
    size_t simplified_volumes = 0;
    size_t skipped_volumes = 0;
};

class ImportJob final : public JobNew {
public:
    using Completion = std::function<void(PreparedImport&&)>;
    ImportJob(std::vector<boost::filesystem::path> paths, ImportJobOptions options, Completion completion);
    static bool supports(const std::vector<boost::filesystem::path>& paths);
    void process(Ctl& ctl) override;
    void finalize(bool canceled, std::exception_ptr& exception) override;
private:
    std::vector<boost::filesystem::path> m_paths;
    ImportJobOptions m_options;
    Completion m_completion;
    PreparedImport m_result;
};

} // namespace Slic3r::GUI
#endif
