#include "libslic3r/LocalSecurity/UnlockLadder.hpp"

#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Slic3r::LocalSecurity;
namespace {
using Seconds = std::chrono::seconds;
using Milliseconds = std::chrono::milliseconds;
unsigned checks = 0;
void check(bool condition, const char* label)
{
    ++checks;
    if (!condition) throw std::runtime_error(label);
}
const Time base = Time{} + std::chrono::hours(2);
const std::string unknown_nonce(32, '0');

// Synthetic fixtures: names in the catalog's shape, not catalog content.
std::vector<LadderDish> pool()
{
    std::vector<LadderDish> dishes;
    for (unsigned i = 1; i <= 8; ++i) {
        LadderDish dish;
        dish.id = "fixture-dish-" + std::to_string(i);
        dish.name_en = "Fixture dish " + std::to_string(i);
        dish.name_zh_hant = "\xE6\xB8\xAC\xE8\xA9\xA6" + std::to_string(i); // 測試N
        dish.alt_en = "Photo of fixture dish " + std::to_string(i);
        dish.alt_yue = "\xE7\x9B\xB8" + std::to_string(i);
        dish.image_file = "fixture-dish-" + std::to_string(i) + ".png";
        dish.photo_ready = i <= 3; // the others may only be wrong choices
        dishes.push_back(dish);
    }
    return dishes;
}
unsigned right_choice(const LadderChallenge& q, const std::vector<LadderDish>& dishes)
{
    const auto dish = std::find_if(dishes.begin(), dishes.end(), [&](const LadderDish& d) { return d.image_file == q.dish.image_file; });
    if (dish == dishes.end()) throw std::runtime_error("question photo is not a catalog dish");
    for (unsigned i = 0; i < q.dish.choices.size(); ++i)
        if (q.dish.choices[i].name_en == dish->name_en) return i;
    throw std::runtime_error("question photo is not among the choices");
}
std::vector<long long> sum_answers(const LadderChallenge& q)
{
    std::vector<long long> answers;
    for (const SumProblem& p : q.sums)
        answers.push_back(p.operation == '+' ? static_cast<long long>(p.left) + p.right : static_cast<long long>(p.left) - p.right);
    return answers;
}
// Starts lockouts until the wait is at least `seconds`; returns the time it began.
Time lock_out(AttemptBudget& budget, Time now, unsigned seconds = 30)
{
    for (;;) {
        for (unsigned i = 0; i < 5; ++i) budget.failed(now);
        const auto wait = budget.state(now).wait_seconds;
        if (wait >= seconds) return now;
        now += Seconds(wait);
    }
}
bool hex_nonce(const std::string& nonce)
{
    return nonce.size() == 32 && std::all_of(nonce.begin(), nonce.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
const auto no_school = [] { return false; };
const auto school = [] { return true; };

void starting_rung()
{
    const auto dishes = pool();
    check(dim_sum_rung_ready(dishes), "a cached photo and three other names make a fair question");
    check(ladder_start(false, true) == LadderRung::DimSum, "the ladder starts with one dish");
    check(ladder_start(true, true) == LadderRung::Sums, "School mode starts at the sums");
    check(ladder_start(true, false) == LadderRung::Sums, "School mode never needs catalog data");
    check(ladder_start(false, false) == LadderRung::Sums, "without a fair dish question the ladder starts at the sums");
    auto no_photo = dishes;
    for (auto& d : no_photo) d.photo_ready = false;
    check(!dim_sum_rung_ready(no_photo), "a photo not on this machine is never the question");
    check(!dim_sum_rung_ready(std::vector<LadderDish>(dishes.begin(), dishes.begin() + 3)), "three dishes cannot make four choices");
    auto shared = dishes;
    for (auto& d : shared) d.name_zh_hant = "\xE5\x90\x8C";
    check(!dim_sum_rung_ready(shared), "choices sharing a name are not a fair question");
    check(!dim_sum_rung_ready({}), "an empty catalog has no dish question");
}

void nothing_to_play()
{
    AttemptBudget budget;
    UnlockLadder ladder(budget, no_school, pool());
    const auto status = ladder.status(base);
    check(!status.waiting && !status.offered && status.rung == LadderRung::Clock, "no ladder without a lockout");
    check(!ladder.challenge(base), "no challenge without a lockout");
    check(ladder.answer_dish(unknown_nonce, 0, base).verdict == LadderVerdict::Rejected, "an unknown nonce is rejected");
    bool rejected = false;
    try { UnlockLadder invalid(budget, {}, pool()); } catch (const Failure&) { rejected = true; }
    check(rejected, "School mode must be readable");
}

void dim_sum_clears_only_the_wait()
{
    const auto dishes = pool();
    LockSession lock({Policy::Pin, Duration::UntilExit, 5});
    for (unsigned i = 0; i < 5; ++i) lock.submit(Factor::Pin, false, base);
    check(lock.attempts(base).wait_seconds == 30 && lock.attempts(base).remaining == 0, "five wrong answers start a 30 second wait");
    UnlockLadder ladder(lock.budget(), no_school, dishes);
    auto status = ladder.status(base);
    check(status.waiting && status.offered && status.rung == LadderRung::DimSum && status.skips_left == 3, "rung one is one dish");
    auto q = ladder.challenge(base);
    check(q && q->rung == LadderRung::DimSum && hex_nonce(q->nonce) && q->expires_in_seconds == ladder_dish_seconds, "dish question with a nonce and an expiry");
    std::set<std::string> names, chinese;
    for (const auto& choice : q->dish.choices) { names.insert(choice.name_en); chinese.insert(choice.name_zh_hant); }
    check(names.size() == 4 && chinese.size() == 4, "four different choices");
    const auto shown = std::find_if(dishes.begin(), dishes.end(), [&](const LadderDish& d) { return d.image_file == q->dish.image_file; });
    check(shown != dishes.end() && shown->photo_ready && q->dish.alt_en == shown->alt_en && q->dish.alt_yue == shown->alt_yue, "the photo is a cached catalog dish with its own alt text");
    auto again = ladder.challenge(base + Seconds(5));
    check(again && again->nonce == q->nonce && again->expires_in_seconds == ladder_dish_seconds - 5, "asking again cannot re-roll the question");
    check(ladder.answer_dish(unknown_nonce, right_choice(*q, dishes), base).verdict == LadderVerdict::Rejected, "a forged nonce is not graded");
    const auto right = right_choice(*q, dishes);
    const auto won = ladder.answer_dish(q->nonce, right, base + Seconds(10));
    check(won.verdict == LadderVerdict::Cleared && !won.status.waiting, "the right dish ends the wait");
    const auto after = lock.attempts(base + Seconds(10));
    check(after.wait_seconds == 0 && after.remaining == 5, "the ladder restores exactly what expiry restores");
    check(lock.locked(base + Seconds(10)) && lock.expected(base + Seconds(10)) == Factor::Pin, "a cleared ladder signs nobody in");
    check(!lock.submit(Factor::Pin, false, base + Seconds(10)) && lock.locked(base + Seconds(10)), "the credential is still required");
    check(ladder.answer_dish(q->nonce, right, base + Seconds(11)).verdict == LadderVerdict::Rejected, "a right answer cannot be replayed");
    check(!ladder.challenge(base + Seconds(11)), "no ladder once the wait is gone");
    for (unsigned i = 0; i < 4; ++i) lock.submit(Factor::Pin, false, base + Seconds(12));
    check(lock.attempts(base + Seconds(12)).wait_seconds == 60, "the skipped lockout still escalates the next wait");
    status = ladder.status(base + Seconds(12));
    check(status.rung == LadderRung::DimSum && status.wrong_dishes == 0 && status.skips_left == 2, "a new lockout starts a fresh ladder");
    auto next = ladder.challenge(base + Seconds(12));
    check(next && next->nonce != q->nonce, "every challenge has its own nonce");
}

void wrong_dishes_reach_the_sums()
{
    const auto dishes = pool();
    AttemptBudget budget;
    lock_out(budget, base);
    UnlockLadder ladder(budget, no_school, dishes);
    for (unsigned i = 0; i < ladder_wrong_dishes; ++i) {
        auto q = ladder.challenge(base);
        check(q && q->rung == LadderRung::DimSum, "dish rung until five wrong dishes");
        const auto right = right_choice(*q, dishes);
        const auto outcome = ladder.answer_dish(q->nonce, (right + 1) % 4, base);
        check(outcome.verdict == LadderVerdict::Wrong && outcome.status.wrong_dishes == i + 1, "a wrong dish is counted");
        check(ladder.answer_dish(q->nonce, right, base).verdict == LadderVerdict::Rejected, "a wrong answer cannot be retried against the same question");
        check(outcome.status.rung == (i + 1 < ladder_wrong_dishes ? LadderRung::DimSum : LadderRung::Sums), "five wrong dishes lead to the sums");
    }
    check(budget.state(base).remaining == 0 && budget.state(base).wait_seconds == 30, "wrong answers never change the wait or the attempts");
    auto q = ladder.challenge(base);
    check(q && q->rung == LadderRung::Sums, "the sums follow");
    check(ladder.answer_dish(q->nonce, 0, base).verdict == LadderVerdict::Wrong && ladder.status(base).rung == LadderRung::Moles, "an answer of the wrong kind is graded wrong");

    AttemptBudget out_of_range;
    lock_out(out_of_range, base);
    UnlockLadder second(out_of_range, no_school, dishes);
    auto dish = second.challenge(base);
    check(second.answer_dish(dish->nonce, 4, base).verdict == LadderVerdict::Wrong, "a choice that does not exist is wrong");
    dish = second.challenge(base);
    check(second.strike(dish->nonce, 0, base).result == StrikeResult::Rejected && second.status(base).wrong_dishes == 2, "a mole strike against a dish question is graded wrong");
    dish = second.challenge(base);
    check(second.answer_sums(dish->nonce, {1, 2, 3}, base).verdict == LadderVerdict::Wrong, "sums against a dish question are graded wrong");
}

void expiry_and_lockout_changes()
{
    const auto dishes = pool();
    AttemptBudget budget;
    const auto start = lock_out(budget, base, 240);
    UnlockLadder ladder(budget, no_school, dishes);
    auto q = ladder.challenge(start);
    const auto late = ladder.answer_dish(q->nonce, right_choice(*q, dishes), start + Seconds(ladder_dish_seconds));
    check(late.verdict == LadderVerdict::Expired && late.status.waiting && late.status.wrong_dishes == 0, "an expired question is not graded");
    check(ladder.answer_dish(q->nonce, right_choice(*q, dishes), start + Seconds(ladder_dish_seconds)).verdict == LadderVerdict::Rejected, "an expired nonce is used up");
    auto fresh = ladder.challenge(start + Seconds(ladder_dish_seconds));
    check(fresh && fresh->nonce != q->nonce, "a new question follows an expired one");
    const auto unanswered = ladder.challenge(start + Seconds(ladder_dish_seconds + 10));
    check(unanswered->nonce == fresh->nonce, "the live question is returned again");
    // The wait ends on its own while a question is open.
    const auto over = start + Seconds(budget.state(start).wait_seconds);
    check(ladder.answer_dish(fresh->nonce, right_choice(*fresh, dishes), over).verdict == LadderVerdict::NotLocked, "nothing to clear after the wait");
    check(budget.state(over).remaining == 5, "ordinary expiry restores the attempts");
    // A question from one lockout is worthless in the next.
    AttemptBudget other;
    const auto first = lock_out(other, base);
    UnlockLadder carried(other, no_school, dishes);
    auto old = carried.challenge(first);
    const auto second = first + Seconds(31);
    lock_out(other, second);
    check(carried.answer_dish(old->nonce, right_choice(*old, dishes), second).verdict == LadderVerdict::Rejected, "an old lockout's nonce is rejected");
    check(other.state(second).wait_seconds == 60, "and the new wait stands");
}

void sums_rung()
{
    AttemptBudget budget;
    lock_out(budget, base);
    UnlockLadder ladder(budget, school, pool());
    auto status = ladder.status(base);
    check(status.offered && status.rung == LadderRung::Sums, "School mode starts at the sums");
    auto q = ladder.challenge(base);
    check(q && q->rung == LadderRung::Sums && q->expires_in_seconds == ladder_sums_seconds, "ten sums");
    check(q->dish.image_file.empty() && q->dish.choices[0].name_en.empty(), "School mode never carries a dish");
    for (const auto& p : q->sums) {
        check(p.operation == '+' || p.operation == '-', "addition or subtraction");
        check(p.left >= 1 && p.left <= 60 && p.right >= 1 && p.right <= 20, "single- and double-digit numbers");
        check(p.operation == '+' || p.right <= p.left, "no negative results");
    }
    auto answers = sum_answers(*q);
    check(ladder.answer_sums(q->nonce, answers, base + Seconds(20)).verdict == LadderVerdict::Cleared, "ten right sums end the wait");
    check(budget.state(base + Seconds(20)).wait_seconds == 0 && budget.state(base + Seconds(20)).remaining == 5, "the sums restore what expiry restores");

    AttemptBudget wrong_budget;
    lock_out(wrong_budget, base);
    UnlockLadder wrong(wrong_budget, school, pool());
    q = wrong.challenge(base);
    answers = sum_answers(*q);
    answers[9] += 1;
    const auto outcome = wrong.answer_sums(q->nonce, answers, base);
    check(outcome.verdict == LadderVerdict::Wrong && outcome.status.rung == LadderRung::Moles, "a single wrong sum leads to the moles");
    check(wrong.answer_sums(q->nonce, sum_answers(*q), base).verdict == LadderVerdict::Rejected, "the sums cannot be retried");
    check(wrong_budget.state(base).wait_seconds > 0, "a wrong sum leaves the wait");

    AttemptBudget short_budget;
    lock_out(short_budget, base);
    UnlockLadder short_list(short_budget, school, pool());
    q = short_list.challenge(base);
    answers = sum_answers(*q);
    answers.pop_back();
    check(short_list.answer_sums(q->nonce, answers, base).verdict == LadderVerdict::Wrong, "nine answers are not ten");

    AttemptBudget slow_budget;
    const auto start = lock_out(slow_budget, base, 480);
    UnlockLadder slow(slow_budget, school, pool());
    q = slow.challenge(start);
    const auto expired = slow.answer_sums(q->nonce, sum_answers(*q), start + Seconds(ladder_sums_seconds));
    check(expired.verdict == LadderVerdict::Expired && expired.status.rung == LadderRung::Sums, "expired sums are not graded");
}

void school_mode_mid_ladder()
{
    const auto dishes = pool();
    bool on = false;
    AttemptBudget budget;
    lock_out(budget, base);
    UnlockLadder ladder(budget, [&] { return on; }, dishes);
    auto q = ladder.challenge(base);
    check(q && q->rung == LadderRung::DimSum, "dish rung outside School mode");
    on = true;
    check(ladder.status(base).rung == LadderRung::Sums, "School mode removes the dish rung at once");
    check(ladder.answer_dish(q->nonce, right_choice(*q, dishes), base).verdict == LadderVerdict::Rejected, "a withdrawn dish question is never graded");
    check(budget.state(base).wait_seconds > 0, "the withdrawn question cleared nothing");
    auto sums = ladder.challenge(base);
    check(sums && sums->rung == LadderRung::Sums && sums->dish.image_file.empty(), "School mode asks sums");
    on = false;
    check(ladder.status(base).rung == LadderRung::Sums && ladder.challenge(base)->nonce == sums->nonce, "rungs never climb back");
    AttemptBudget direct;
    lock_out(direct, base);
    UnlockLadder answered(direct, [&] { return on; }, dishes);
    q = answered.challenge(base);
    on = true;
    check(answered.answer_dish(q->nonce, right_choice(*q, dishes), base).verdict == LadderVerdict::Rejected && direct.state(base).wait_seconds > 0,
          "a dish answer arriving after School mode switched on is never graded");
    on = false;
    AttemptBudget empty;
    lock_out(empty, base);
    UnlockLadder no_catalog(empty, no_school, {});
    check(no_catalog.status(base).rung == LadderRung::Sums, "no catalog data starts at the sums");
}

// Reaches the moles through one wrong sum.
std::optional<LadderChallenge> moles(UnlockLadder& ladder, Time now)
{
    auto q = ladder.challenge(now);
    auto answers = sum_answers(*q);
    answers[0] += 1;
    ladder.answer_sums(q->nonce, answers, now);
    return ladder.challenge(now);
}

void mole_round_rules()
{
    AttemptBudget budget;
    const auto now = lock_out(budget, base, 120);
    UnlockLadder ladder(budget, school, pool());
    const auto q = moles(ladder, now);
    check(q && q->rung == LadderRung::Moles && hex_nonce(q->nonce), "whack-a-mole after a wrong sum");
    const auto& round = q->round;
    check(round.lead_in_ms == ladder_round_lead_in_ms && round.duration_ms == ladder_round_ms && round.needed == ladder_moles_needed &&
          round.strike_limit == ladder_strike_limit && round.moles.size() == ladder_mole_count, "the published round");
    for (std::size_t i = 0; i < round.moles.size(); ++i) {
        check(round.moles[i].cell < ladder_mole_cells && round.moles[i].visible_ms == ladder_mole_visible_ms, "moles in the grid");
        check(round.moles[i].appears_ms + round.moles[i].visible_ms <= round.duration_ms, "moles inside the round");
        if (i) check(round.moles[i].cell != round.moles[i - 1].cell && round.moles[i].appears_ms >= round.moles[i - 1].appears_ms + round.moles[i - 1].visible_ms, "moles never overlap");
    }
    check(ladder.status(now).rung == LadderRung::Moles, "the round is in play");
    const Time start = now + Milliseconds(round.lead_in_ms);
    auto at = [&](unsigned ms) { return start + Milliseconds(ms); };
    const Mole& first = round.moles[0];
    check(ladder.strike(q->nonce, first.cell, now + Milliseconds(500)).result == StrikeResult::OutsideRound, "no hits before the round starts");
    check(ladder.strike(q->nonce, (first.cell + 1) % ladder_mole_cells, at(first.appears_ms + 100)).result == StrikeResult::Empty, "an empty cell is a miss");
    auto hit = ladder.strike(q->nonce, first.cell, at(first.appears_ms + 200));
    check(hit.result == StrikeResult::Hit && hit.hits == 1, "a visible mole counts");
    for (unsigned i = 0; i < 5; ++i) {
        const auto spam = ladder.strike(q->nonce, first.cell, at(first.appears_ms + 300 + i));
        check(spam.result == StrikeResult::Repeat && spam.hits == 1, "each mole counts once");
    }
    check(ladder.strike(q->nonce, first.cell, at(first.appears_ms + first.visible_ms)).result != StrikeResult::Hit, "a mole that has gone cannot be hit");
    check(ladder.strike(q->nonce, ladder_mole_cells, at(round.moles[1].appears_ms)).result == StrikeResult::Empty, "a cell outside the grid is a miss");
    const auto early = ladder.finish_round(q->nonce, at(round.duration_ms - 1));
    check(early.verdict == LadderVerdict::Early && early.status.rung == LadderRung::Clock && !early.status.offered, "a round cannot be won faster than it lasts");
    check(ladder.finish_round(q->nonce, at(round.duration_ms)).verdict == LadderVerdict::Rejected, "the round's nonce is used up");
    check(ladder.strike(q->nonce, first.cell, at(1000)).result == StrikeResult::Rejected, "no strikes after the round is handed in");
    check(!ladder.challenge(at(round.duration_ms)), "the clock follows a lost round");
    check(budget.state(at(round.duration_ms)).wait_seconds > 0, "the wait goes on");
}

void mole_round_won_and_lost()
{
    for (const unsigned hits : {ladder_mole_count, ladder_moles_needed, ladder_moles_needed - 1}) {
        AttemptBudget budget;
        const auto now = lock_out(budget, base, 60);
        UnlockLadder ladder(budget, school, pool());
        const auto q = moles(ladder, now);
        const Time start = now + Milliseconds(q->round.lead_in_ms);
        unsigned live = 0;
        for (unsigned i = 0; i < hits; ++i) {
            const auto& mole = q->round.moles[i];
            const auto strike = ladder.strike(q->nonce, mole.cell, start + Milliseconds(mole.appears_ms + mole.visible_ms - 1));
            check(strike.result == StrikeResult::Hit && strike.hits == ++live, "live score");
        }
        check(ladder.strike(q->nonce, q->round.moles[0].cell, start + Milliseconds(q->round.duration_ms)).result == StrikeResult::OutsideRound, "no hits after the round");
        const auto outcome = ladder.finish_round(q->nonce, start + Milliseconds(q->round.duration_ms));
        if (hits >= ladder_moles_needed) {
            check(outcome.verdict == LadderVerdict::Cleared && budget.state(start + Milliseconds(q->round.duration_ms)).remaining == 5, "enough moles end the wait");
        } else {
            check(outcome.verdict == LadderVerdict::Wrong && outcome.status.rung == LadderRung::Clock && outcome.status.waiting, "too few moles lose the round");
            check(!ladder.challenge(start + Milliseconds(q->round.duration_ms)), "the ladder is not offered again for this lockout");
        }
    }
}

void mole_round_abuse()
{
    AttemptBudget budget;
    const auto now = lock_out(budget, base, 60);
    UnlockLadder ladder(budget, school, pool());
    const auto q = moles(ladder, now);
    const Time start = now + Milliseconds(q->round.lead_in_ms);
    const auto& first = q->round.moles[0];
    const unsigned empty = (first.cell + 1) % ladder_mole_cells;
    for (unsigned i = 0; i < q->round.strike_limit; ++i)
        check(ladder.strike(q->nonce, empty, start + Milliseconds(first.appears_ms + i)).result == StrikeResult::Empty, "misses spend strikes");
    const auto spent = ladder.strike(q->nonce, first.cell, start + Milliseconds(first.appears_ms + 100));
    check(spent.result == StrikeResult::Rejected && spent.hits == 0 && spent.strikes_left == 0, "sending enough taps is not hitting moles");
    check(ladder.finish_round(q->nonce, start + Milliseconds(q->round.duration_ms)).verdict == LadderVerdict::Wrong, "a spammed round is lost");

    // Walking away, restarting or answering the wrong kind all lose the round.
    AttemptBudget walked;
    const auto t = lock_out(walked, base, 120);
    UnlockLadder away(walked, school, pool());
    const auto round = moles(away, t);
    UnlockLadder restarted(walked, school, pool());
    check(restarted.status(t).rung == LadderRung::Clock && !restarted.challenge(t), "a new ladder cannot replay an issued round");
    const auto expires = t + Milliseconds(round->round.lead_in_ms + round->round.duration_ms + ladder_round_grace_ms);
    check(away.status(expires).rung == LadderRung::Clock && !away.challenge(expires), "an abandoned round is lost");
    check(away.finish_round(round->nonce, expires).verdict == LadderVerdict::Rejected, "an abandoned round is withdrawn");

    AttemptBudget mixed;
    const auto m = lock_out(mixed, base, 60);
    UnlockLadder kinds(mixed, school, pool());
    const auto wrong_kind = moles(kinds, m);
    check(kinds.answer_sums(wrong_kind->nonce, sum_answers(*wrong_kind), m).verdict == LadderVerdict::Wrong && kinds.status(m).rung == LadderRung::Clock, "sums against a round lose it");

    AttemptBudget handed;
    const auto h = lock_out(handed, base, 120);
    UnlockLadder late(handed, school, pool());
    const auto slow = moles(late, h);
    const auto deadline = h + Milliseconds(slow->round.lead_in_ms + slow->round.duration_ms + ladder_round_grace_ms);
    check(late.finish_round(slow->nonce, deadline).verdict == LadderVerdict::Expired, "a round handed in after its grace has expired");
}

void hourly_cap()
{
    const auto dishes = pool();
    AttemptBudget budget;
    UnlockLadder ladder(budget, no_school, dishes);
    Time now = base;
    for (unsigned round = 0; round < 3; ++round) {
        lock_out(budget, now);
        check(ladder.status(now).skips_left == 3 - round, "allowance counts down");
        auto q = ladder.challenge(now);
        check(ladder.answer_dish(q->nonce, right_choice(*q, dishes), now).verdict == LadderVerdict::Cleared, "a ladder win within the cap");
        now += Seconds(60);
    }
    lock_out(budget, now);
    const auto status = ladder.status(now);
    check(status.waiting && !status.offered && status.skips_left == 0, "three skips an hour, then only the clock");
    check(!ladder.challenge(now), "no challenge once the allowance is spent");
    check(budget.state(now).wait_seconds == 240, "escalation continued through every skip");
    const auto refill = base + std::chrono::hours(1);
    lock_out(budget, refill);
    check(ladder.status(refill).offered && ladder.status(refill).skips_left == 1, "the oldest skip leaves the rolling hour");
}

void answer_positions_vary()
{
    const auto dishes = pool();
    std::set<unsigned> positions;
    for (unsigned i = 0; i < 40; ++i) {
        AttemptBudget budget;
        lock_out(budget, base);
        UnlockLadder ladder(budget, no_school, dishes);
        positions.insert(right_choice(*ladder.challenge(base), dishes));
    }
    check(positions.size() > 1, "the right answer is not in a fixed place");
    // A non-dish answer carries no choice, so it must never pass for the first one.
    unsigned graded = 0;
    for (unsigned i = 0; i < 400 && graded < 3; ++i) {
        AttemptBudget budget;
        lock_out(budget, base);
        UnlockLadder ladder(budget, no_school, dishes);
        const auto q = ladder.challenge(base);
        if (right_choice(*q, dishes) != 0) continue;
        LadderVerdict verdict = LadderVerdict::Cleared;
        if (graded == 0) verdict = ladder.finish_round(q->nonce, base).verdict;
        else if (graded == 1) verdict = ladder.answer_sums(q->nonce, {}, base).verdict;
        else {
            check(ladder.strike(q->nonce, 0, base).result == StrikeResult::Rejected, "a strike is not a dish answer");
            if (ladder.status(base).wrong_dishes == 1) verdict = LadderVerdict::Wrong;
        }
        check(verdict == LadderVerdict::Wrong && budget.state(base).wait_seconds > 0, "an answer of the wrong kind never matches the first choice");
        ++graded;
    }
    check(graded == 3, "every wrong kind was tried against a first-choice question");
}
} // namespace

int main()
{
    try {
        starting_rung();
        nothing_to_play();
        dim_sum_clears_only_the_wait();
        wrong_dishes_reach_the_sums();
        expiry_and_lockout_changes();
        sums_rung();
        school_mode_mid_ladder();
        mole_round_rules();
        mole_round_won_and_lost();
        mole_round_abuse();
        hourly_cap();
        answer_positions_vary();
        std::cout << "PASS " << checks << " unlock ladder checks\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
