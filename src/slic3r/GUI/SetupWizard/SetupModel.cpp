#include "SetupModel.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>

#ifndef L
#define L(s) s
#endif

namespace Slic3r { namespace GUI { namespace Setup {

const std::array<StepInfo, kStepCount> &steps()
{
    static const std::array<StepInfo, kStepCount> all = {{
        {Step::Language, "language", L("Language and humour"), false, false, true},
        {Step::Look, "look", L("Look and motion"), false, false, true},
        {Step::Printers, "printers", L("Printers"), true, false, true},
        {Step::Inks, "inks", L("Inks and filaments"), true, false, true},
        {Step::Backups, "backups", L("Backups"), false, true, true},
        // Designed with the Extensions board; offered here the day extensions ship.
        {Step::Extensions, "extensions", L("Extensions"), false, true, false},
        {Step::Finish, "finish", L("Finish"), false, false, true},
    }};
    return all;
}

const StepInfo &info(int index) { return steps()[std::size_t(std::clamp(index, 0, kStepCount - 1))]; }

int index_of(Step step) { return int(step); }

std::string serialize(const Progress &p)
{
    nlohmann::json states = nlohmann::json::object();
    for (int i = 0; i < kStepCount; ++i) {
        const State s = p.states[std::size_t(i)];
        states[steps()[std::size_t(i)].id] = s == State::Done ? "done" : s == State::Skipped ? "skipped" : "pending";
    }
    return nlohmann::json{{"current", steps()[std::size_t(std::clamp(p.current, 0, kStepCount - 1))].id},
                          {"states", states},
                          {"updated", p.updated},
                          {"finished", p.finished},
                          {"first_run", p.first_run}}
        .dump();
}

bool parse(std::string_view json, Progress &out)
{
    const nlohmann::json j = nlohmann::json::parse(json.begin(), json.end(), nullptr, false);
    if (j.is_discarded() || !j.is_object()) return false;
    Progress p;
    if (j.contains("current") && j["current"].is_string()) {
        const std::string id = j["current"].get<std::string>();
        for (int i = 0; i < kStepCount; ++i)
            if (id == steps()[std::size_t(i)].id) p.current = i;
    }
    if (j.contains("states") && j["states"].is_object()) {
        for (int i = 0; i < kStepCount; ++i) {
            const char *id = steps()[std::size_t(i)].id;
            if (!j["states"].contains(id) || !j["states"][id].is_string()) continue;
            const std::string s = j["states"][id].get<std::string>();
            if (s == "done") p.states[std::size_t(i)] = State::Done;
            // Only an optional step can stay skipped, whatever the file says.
            else if (s == "skipped" && steps()[std::size_t(i)].optional) p.states[std::size_t(i)] = State::Skipped;
        }
    }
    if (j.contains("updated") && j["updated"].is_number_integer()) p.updated = j["updated"].get<std::int64_t>();
    if (j.contains("finished") && j["finished"].is_boolean()) p.finished = j["finished"].get<bool>();
    if (j.contains("first_run") && j["first_run"].is_boolean()) p.first_run = j["first_run"].get<bool>();
    if (!info(p.current).available) p.current = next_index(p.current);
    out = p;
    return true;
}

Missing missing_for(Step step, const Facts &facts)
{
    if (step == Step::Printers && facts.printers < 1) return Missing::Printer;
    if (step == Step::Inks && facts.filaments < 1) return Missing::Filament;
    return Missing::None;
}

int required_left(const Facts &facts)
{
    int left = 0;
    for (const StepInfo &s : steps())
        if (s.required && s.available && missing_for(s.step, facts) != Missing::None) ++left;
    return left;
}

bool can_finish(const Facts &facts) { return required_left(facts) == 0; }

int next_index(int from)
{
    for (int i = from + 1; i < kStepCount; ++i)
        if (steps()[std::size_t(i)].available) return i;
    return kStepCount - 1;
}

int previous_index(int from)
{
    for (int i = from - 1; i >= 0; --i)
        if (steps()[std::size_t(i)].available) return i;
    return std::max(0, std::min(from, kStepCount - 1));
}

int resume_index(const Progress &p)
{
    const int current = std::clamp(p.current, 0, kStepCount - 1);
    if (p.states[std::size_t(current)] == State::Pending && info(current).available) return current;
    for (int i = 0; i < kStepCount; ++i)
        if (steps()[std::size_t(i)].available && p.states[std::size_t(i)] == State::Pending) return i;
    return kStepCount - 1;
}

int done_count(const Progress &p)
{
    return int(std::count(p.states.begin(), p.states.end(), State::Done));
}

bool has_saved_work(const Progress &p) { return !p.finished && p.updated != 0 && (p.current > 0 || done_count(p) > 0); }

Progress complete(Progress p, int index, std::int64_t now)
{
    index = std::clamp(index, 0, kStepCount - 1);
    p.states[std::size_t(index)] = State::Done;
    p.current                    = next_index(index);
    p.updated                    = now;
    return p;
}

Progress skip(Progress p, int index, std::int64_t now)
{
    index = std::clamp(index, 0, kStepCount - 1);
#ifndef SLIC3R_SETUP_MODEL_NEGATIVE_FIXTURE
    if (!info(index).optional) return p;
#endif
    p.states[std::size_t(index)] = State::Skipped;
    p.current                    = next_index(index);
    p.updated                    = now;
    return p;
}

Progress go_to(Progress p, int index, std::int64_t now)
{
    index = std::clamp(index, 0, kStepCount - 1);
    if (!info(index).available) index = next_index(index);
    p.current = index;
    p.updated = now;
    return p;
}

Progress finish(Progress p, std::int64_t now)
{
    p.states[std::size_t(index_of(Step::Finish))] = State::Done;
    p.current                                     = index_of(Step::Finish);
    p.finished                                    = true;
    p.updated                                     = now;
    return p;
}

Progress start_over(bool first_run, std::int64_t now)
{
    Progress p;
    p.first_run = first_run;
    p.updated   = now;
    return p;
}

}}} // namespace Slic3r::GUI::Setup
