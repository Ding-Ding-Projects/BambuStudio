#pragma once

#include <memory>
#include <utility>

namespace Slic3r { namespace GUI {
enum class ProjectLoadMode { Interactive, AutomaticStartup };
enum class ProjectRestoreResult { Failed, InteractionRequired, ApplicationStarted };

struct ProjectLoadContext {
    ProjectLoadMode mode = ProjectLoadMode::Interactive;
    ProjectRestoreResult* result = nullptr;
    bool automatic() const { return mode == ProjectLoadMode::AutomaticStartup; }
    bool defer() const {
        if (result) *result = ProjectRestoreResult::InteractionRequired;
        return false;
    }
};

// Automatic restore does not create a dialog at all. Interactive callers keep
// the existing progress implementation, including cancellation and visibility.
template<class Dialog> class ProjectLoadProgress {
    std::unique_ptr<Dialog> dialog;
public:
    template<class Factory> ProjectLoadProgress(ProjectLoadContext context, Factory factory) {
        if (!context.automatic()) dialog = factory();
    }
    template<class... Args> bool Update(Args&&... args) {
        return !dialog || dialog->Update(std::forward<Args>(args)...);
    }
    bool WasCancelled() const { return dialog && dialog->WasCancelled(); }
    void Show(bool visible) { if (dialog) dialog->Show(visible); }
    void Hide() { if (dialog) dialog->Hide(); }
    void Raise() { if (dialog) dialog->Raise(); }
};
}}
