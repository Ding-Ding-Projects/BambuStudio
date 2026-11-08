// Standalone assertions for src/slic3r/GUI/SetupWizard/SetupModel.cpp, the
// practical setup wizard's rules. Compiled and run by tests/setup_model.test.mjs.
#include "src/slic3r/GUI/SetupWizard/SetupModel.cpp"

#include <cstdio>

using namespace Slic3r::GUI::Setup;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

static void the_steps()
{
    CHECK(steps().size() == 7);
    CHECK(info(index_of(Step::Printers)).required && info(index_of(Step::Inks)).required);
    CHECK(info(index_of(Step::Backups)).optional && !info(index_of(Step::Printers)).optional);
    // Every step is built now, Extensions included, so navigation visits each in turn.
    for (const StepInfo &s : steps()) CHECK(s.available);
    CHECK(info(index_of(Step::Extensions)).optional && !info(index_of(Step::Extensions)).required);
    CHECK(next_index(index_of(Step::Backups)) == index_of(Step::Extensions));
    CHECK(previous_index(index_of(Step::Finish)) == index_of(Step::Extensions));
    CHECK(next_index(index_of(Step::Finish)) == index_of(Step::Finish));
    CHECK(previous_index(0) == 0);
    for (const StepInfo &s : steps()) CHECK(!(s.required && s.optional));
}

static void required_facts()
{
    Facts none;
    CHECK(missing_for(Step::Printers, none) == Missing::Printer);
    CHECK(missing_for(Step::Inks, none) == Missing::Filament);
    CHECK(missing_for(Step::Backups, none) == Missing::None);
    CHECK(required_left(none) == 2 && !can_finish(none));
    Facts some{1, 0};
    CHECK(required_left(some) == 1);
    Facts all{2, 9};
    CHECK(required_left(all) == 0 && can_finish(all));
}

static void moving_and_skipping()
{
    Progress p = start_over(true, 100);
    CHECK(p.first_run && p.updated == 100 && p.current == 0 && !has_saved_work(p));
    p = complete(p, 0, 101);
    CHECK(p.states[0] == State::Done && p.current == 1 && p.updated == 101);
    CHECK(has_saved_work(p) && done_count(p) == 1);
    // A required step cannot be skipped.
    const Progress refused = skip(p, index_of(Step::Printers), 102);
    CHECK(refused.states[std::size_t(index_of(Step::Printers))] == State::Pending);
    CHECK(refused.current == p.current && refused.updated == p.updated);
    // An optional one can, and the next step is Extensions, which can be skipped too.
    p = skip(go_to(p, index_of(Step::Backups), 103), index_of(Step::Backups), 104);
    CHECK(p.states[std::size_t(index_of(Step::Backups))] == State::Skipped);
    CHECK(p.current == index_of(Step::Extensions));
    p = skip(p, index_of(Step::Extensions), 105);
    CHECK(p.states[std::size_t(index_of(Step::Extensions))] == State::Skipped && p.current == index_of(Step::Finish));
    p = finish(p, 106);
    CHECK(p.finished && !has_saved_work(p));
}

static void resuming()
{
    Progress p = start_over(false, 1);
    p          = complete(p, 0, 2);
    p          = complete(p, 1, 3);
    CHECK(resume_index(p) == 2);
    p.current = 0; // pointing at a done step: continue at the first pending one
    CHECK(resume_index(p) == 2);
    p = complete(complete(complete(p, 2, 4), 3, 5), 4, 6);
    CHECK(resume_index(p) == index_of(Step::Extensions));
    p = complete(p, index_of(Step::Extensions), 7);
    CHECK(resume_index(p) == index_of(Step::Finish));
}

static void saving()
{
    Progress p = start_over(true, 1762642692);
    p          = complete(complete(p, 0, 1762642693), 1, 1762642694);
    p          = skip(go_to(p, 4, 1762642695), 4, 1762642696);
    Progress back;
    CHECK(parse(serialize(p), back));
    CHECK(back.current == p.current && back.states == p.states && back.updated == p.updated);
    CHECK(back.first_run && !back.finished);
    CHECK(!parse("not json", back) && !parse("[1,2]", back));
    // A hand-edited file cannot mark a required step skipped; unknown steps are ignored.
    Progress odd;
    CHECK(parse(R"({"current":"extensions","states":{"printers":"skipped","language":"done","nonsense":"done"}})", odd));
    CHECK(odd.states[std::size_t(index_of(Step::Printers))] == State::Pending);
    CHECK(odd.states[0] == State::Done);
    CHECK(odd.current == index_of(Step::Extensions));
}

int main()
{
    the_steps();
    required_facts();
    moving_and_skipping();
    resuming();
    saving();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("setup model: all checks passed\n");
    return 0;
}
