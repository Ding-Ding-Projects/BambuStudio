#include "UnlockLadder.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <openssl/crypto.h>
#include <openssl/rand.h>

namespace Slic3r::LocalSecurity {
namespace {
using Milliseconds = std::chrono::milliseconds;

void require(bool ok, Error e = Error::InvalidInput) { if (!ok) throw Failure(e); }

// Uniform in [0, bound), from OpenSSL's CSPRNG so no question can be predicted.
unsigned uniform(std::size_t bound)
{
    require(bound > 0 && bound <= std::numeric_limits<std::uint32_t>::max());
    const std::uint32_t span = static_cast<std::uint32_t>(bound);
    const std::uint32_t top = std::numeric_limits<std::uint32_t>::max();
    const std::uint32_t limit = top - top % span;
    for (;;) {
        unsigned char bytes[4];
        require(RAND_bytes(bytes, 4) == 1, Error::Unavailable);
        const std::uint32_t value = (std::uint32_t(bytes[0]) << 24) | (std::uint32_t(bytes[1]) << 16) |
                                    (std::uint32_t(bytes[2]) << 8) | std::uint32_t(bytes[3]);
        if (value < limit) return static_cast<unsigned>(value % span);
    }
}
unsigned between(unsigned low, unsigned high) { return low + uniform(std::size_t(high - low) + 1); }

bool same_nonce(const std::string& expected, std::string_view offered)
{
    return !expected.empty() && expected.size() == offered.size() &&
           CRYPTO_memcmp(expected.data(), offered.data(), expected.size()) == 0;
}
unsigned seconds_until(Time deadline, Time now)
{
    if (deadline <= now) return 0;
    return static_cast<unsigned>(std::chrono::ceil<std::chrono::seconds>(deadline - now).count());
}
bool same_progress(const LadderProgress& a, const LadderProgress& b)
{
    return a.started == b.started && a.rung == b.rung && a.wrong_dishes == b.wrong_dishes;
}

bool usable(const LadderDish& dish)
{
    return !dish.name_en.empty() && !dish.name_zh_hant.empty() && dish.name_en.size() <= 256 &&
           dish.name_zh_hant.size() <= 256 && dish.alt_en.size() <= 1024 && dish.alt_yue.size() <= 1024;
}
bool question_dish(const LadderDish& dish)
{
    return usable(dish) && dish.photo_ready && !dish.image_file.empty() && dish.image_file.size() <= 256;
}
// Two choices that share either name would make the question unfair.
bool distinct(const LadderDish& a, const LadderDish& b)
{
    return a.name_en != b.name_en && a.name_zh_hant != b.name_zh_hant;
}
bool fits(const std::vector<LadderDish>& dishes, std::size_t answer, const std::vector<std::size_t>& picked, std::size_t candidate)
{
    if (candidate == answer || !usable(dishes[candidate]) || !distinct(dishes[candidate], dishes[answer])) return false;
    return std::all_of(picked.begin(), picked.end(), [&](std::size_t other) { return distinct(dishes[candidate], dishes[other]); });
}
// Deterministic scan from `start`; used for readiness and as the random draw's fallback.
std::vector<std::size_t> scan_wrong_choices(const std::vector<LadderDish>& dishes, std::size_t answer, std::size_t start)
{
    std::vector<std::size_t> picked;
    for (std::size_t step = 0; step < dishes.size() && picked.size() + 1 < ladder_dish_choices; ++step) {
        const std::size_t candidate = (start + step) % dishes.size();
        if (fits(dishes, answer, picked, candidate)) picked.push_back(candidate);
    }
    return picked;
}
std::vector<std::size_t> draw_wrong_choices(const std::vector<LadderDish>& dishes, std::size_t answer)
{
    std::vector<std::size_t> picked;
    for (unsigned draw = 0; draw < 64 && picked.size() + 1 < ladder_dish_choices; ++draw) {
        const std::size_t candidate = uniform(dishes.size());
        if (fits(dishes, answer, picked, candidate)) picked.push_back(candidate);
    }
    if (picked.size() + 1 < ladder_dish_choices) picked = scan_wrong_choices(dishes, answer, uniform(dishes.size()));
    return picked;
}
} // namespace

bool dim_sum_rung_ready(const std::vector<LadderDish>& dishes)
{
    for (std::size_t answer = 0; answer < dishes.size(); ++answer)
        if (question_dish(dishes[answer]) && scan_wrong_choices(dishes, answer, 0).size() + 1 == ladder_dish_choices)
            return true;
    return false;
}

LadderRung ladder_start(bool school_mode, bool dim_sum_ready) noexcept
{
    return school_mode || !dim_sum_ready ? LadderRung::Sums : LadderRung::DimSum;
}

struct UnlockLadder::Pending {
    LadderChallenge challenge;
    std::uint64_t lockout = 0;
    Time expires{};
    Time round_start{};
    unsigned answer = 0;
    std::array<long long, ladder_sum_count> sums{};
    std::vector<bool> hit;
    unsigned hits = 0;
    unsigned strikes = 0;
};

UnlockLadder::UnlockLadder(AttemptBudget& budget, std::function<bool()> school_mode, std::vector<LadderDish> dishes)
    : m_budget(budget), m_school_mode(std::move(school_mode))
{
    require(bool(m_school_mode));
    set_dishes(std::move(dishes));
}
UnlockLadder::~UnlockLadder() = default;

void UnlockLadder::set_dishes(std::vector<LadderDish> dishes)
{
    require(dishes.size() <= 100000);
    m_dishes = std::move(dishes);
    m_dim_sum_ready = dim_sum_rung_ready(m_dishes);
}

UnlockLadder::Context UnlockLadder::context(Time now)
{
    Context c;
    c.attempts = m_budget.state(now);
    c.waiting = c.attempts.wait_seconds > 0;
    if (!c.waiting) {
        m_pending.reset();
        return c;
    }
    if (m_pending && m_pending->lockout != c.attempts.lockout) m_pending.reset();
    c.progress = m_budget.ladder(now);
    // Rungs only ever move down. School mode switching on mid-ladder removes
    // the dim-sum rung at once; switching it off never brings it back.
    const LadderRung start = ladder_start(m_school_mode(), m_dim_sum_ready);
    LadderProgress next = c.progress;
    if (!next.started) next = LadderProgress{true, start, 0};
    else if (next.rung < start) next.rung = start;
    if (!same_progress(next, c.progress)) {
        m_budget.set_ladder(c.attempts.lockout, next, now);
        c.progress = next;
    }
    if (m_pending && m_pending->challenge.rung != LadderRung::Moles && m_pending->challenge.rung != c.progress.rung)
        m_pending.reset(); // withdrawn, never answered
    return c;
}

LadderStatus UnlockLadder::make_status(const Context& c, Time now)
{
    LadderStatus status;
    status.waiting = c.waiting;
    status.wait_seconds = c.attempts.wait_seconds;
    status.remaining_attempts = c.attempts.remaining;
    status.skips_left = m_budget.skips_left(now);
    if (!c.waiting) return status;
    status.rung = c.progress.rung;
    status.wrong_dishes = c.progress.wrong_dishes;
    if (m_pending && m_pending->challenge.rung == LadderRung::Moles && now < m_pending->expires)
        status.rung = LadderRung::Moles; // a round in play; its rung is already spent
    status.offered = status.rung != LadderRung::Clock && status.skips_left > 0;
    return status;
}

LadderStatus UnlockLadder::status(Time now) { return make_status(context(now), now); }

LadderOutcome UnlockLadder::result(LadderVerdict verdict, Time now)
{
    LadderOutcome outcome;
    outcome.verdict = verdict;
    outcome.status = status(now);
    return outcome;
}

std::optional<LadderChallenge> UnlockLadder::challenge(Time now)
{
    const Context c = context(now);
    if (!c.waiting || m_budget.skips_left(now) == 0) return std::nullopt;
    if (m_pending && now < m_pending->expires) {
        LadderChallenge again = m_pending->challenge;
        again.expires_in_seconds = seconds_until(m_pending->expires, now);
        return again;
    }
    m_pending.reset(); // expired; an expired round was already recorded as spent
    if (c.progress.rung == LadderRung::Clock) return std::nullopt;
    return issue(c, now);
}

LadderChallenge UnlockLadder::issue(const Context& c, Time now)
{
    auto pending = std::make_unique<Pending>();
    pending->lockout = c.attempts.lockout;
    LadderChallenge& q = pending->challenge;
    q.nonce = new_stable_id();
    q.rung = c.progress.rung;
    switch (q.rung) {
    case LadderRung::DimSum: {
        std::vector<std::size_t> candidates;
        for (std::size_t i = 0; i < m_dishes.size(); ++i)
            if (question_dish(m_dishes[i])) candidates.push_back(i);
        std::size_t answer = m_dishes.size();
        std::vector<std::size_t> wrong;
        for (unsigned draw = 0; draw < 16 && answer == m_dishes.size() && !candidates.empty(); ++draw) {
            const std::size_t candidate = candidates[uniform(candidates.size())];
            wrong = draw_wrong_choices(m_dishes, candidate);
            if (wrong.size() + 1 == ladder_dish_choices) answer = candidate;
        }
        for (std::size_t i = 0; i < candidates.size() && answer == m_dishes.size(); ++i) {
            wrong = scan_wrong_choices(m_dishes, candidates[i], uniform(m_dishes.size()));
            if (wrong.size() + 1 == ladder_dish_choices) answer = candidates[i];
        }
        require(answer < m_dishes.size(), Error::Unavailable);
        const LadderDish& dish = m_dishes[answer];
        q.dish.image_file = dish.image_file;
        q.dish.alt_en = dish.alt_en;
        q.dish.alt_yue = dish.alt_yue;
        pending->answer = uniform(ladder_dish_choices);
        for (unsigned slot = 0, next = 0; slot < ladder_dish_choices; ++slot) {
            const LadderDish& shown = slot == pending->answer ? dish : m_dishes[wrong[next++]];
            q.dish.choices[slot] = DishChoice{shown.name_en, shown.name_zh_hant};
        }
        pending->expires = now + std::chrono::seconds(ladder_dish_seconds);
        break;
    }
    case LadderRung::Sums:
        // Single- and double-digit sums; nothing anybody needs paper for.
        for (unsigned i = 0; i < ladder_sum_count; ++i) {
            SumProblem& problem = q.sums[i];
            if (uniform(2) == 0) {
                problem.operation = '+';
                problem.left = between(1, 40);
                problem.right = between(1, 20);
                pending->sums[i] = static_cast<long long>(problem.left) + problem.right;
            } else {
                problem.operation = '-';
                problem.left = between(5, 60);
                problem.right = between(1, std::min(20u, problem.left));
                pending->sums[i] = static_cast<long long>(problem.left) - problem.right;
            }
        }
        pending->expires = now + std::chrono::seconds(ladder_sums_seconds);
        break;
    case LadderRung::Moles: {
        MoleRound& round = q.round;
        round.lead_in_ms = ladder_round_lead_in_ms;
        round.duration_ms = ladder_round_ms;
        round.needed = ladder_moles_needed;
        round.strike_limit = ladder_strike_limit;
        unsigned previous = ladder_mole_cells;
        for (unsigned i = 0; i < ladder_mole_count; ++i) {
            unsigned cell = 0;
            if (previous == ladder_mole_cells) cell = uniform(ladder_mole_cells);
            else {
                cell = uniform(ladder_mole_cells - 1);
                if (cell >= previous) ++cell; // never the same cell twice running
            }
            round.moles.push_back(Mole{cell, i * ladder_mole_spacing_ms, ladder_mole_visible_ms});
            previous = cell;
        }
        pending->hit.assign(round.moles.size(), false);
        pending->round_start = now + Milliseconds(round.lead_in_ms);
        pending->expires = pending->round_start + Milliseconds(round.duration_ms + ladder_round_grace_ms);
        // Issuing the round spends the rung, so abandoning, restarting or
        // replacing a round is a lost round and the clock follows.
        LadderProgress spent = c.progress;
        spent.rung = LadderRung::Clock;
        m_budget.set_ladder(c.attempts.lockout, spent, now);
        break;
    }
    case LadderRung::Clock:
        throw Failure(Error::InvalidInput);
    }
    q.expires_in_seconds = seconds_until(pending->expires, now);
    m_pending = std::move(pending);
    return m_pending->challenge;
}

LadderOutcome UnlockLadder::win(Time now)
{
    // clear_wait owns the hourly cap and restores exactly the attempts that
    // ordinary expiry restores. It touches no lock session and no escalation.
    return result(m_budget.clear_wait(now) ? LadderVerdict::Cleared : LadderVerdict::CapReached, now);
}

LadderOutcome UnlockLadder::wrong(const Context& c, LadderRung rung, Time now)
{
    LadderProgress next = c.progress;
    if (rung == LadderRung::DimSum) {
        next.wrong_dishes = std::min(ladder_wrong_dishes, next.wrong_dishes + 1);
        if (next.wrong_dishes >= ladder_wrong_dishes) next.rung = LadderRung::Sums;
    } else if (rung == LadderRung::Sums) {
        next.rung = LadderRung::Moles; // a single wrong sum
    } else {
        next.rung = LadderRung::Clock; // a lost round; no ladder for this lockout
    }
    m_budget.set_ladder(c.attempts.lockout, next, now);
    return result(LadderVerdict::Wrong, now);
}

LadderOutcome UnlockLadder::grade(std::string_view nonce, Kind kind, unsigned choice, const std::vector<long long>* answers, Time now)
{
    if (!m_pending || !same_nonce(m_pending->challenge.nonce, nonce)) return result(LadderVerdict::Rejected, now);
    // Consumed before grading: a wrong answer cannot be retried against the
    // same question and a right one cannot be replayed.
    const std::unique_ptr<Pending> pending = std::move(m_pending);
    const Context c = context(now);
    if (!c.waiting) return result(LadderVerdict::NotLocked, now);
    if (pending->lockout != c.attempts.lockout) return result(LadderVerdict::Rejected, now);
    const LadderRung rung = pending->challenge.rung;
    if (rung == LadderRung::Moles) {
        if (kind != Kind::Finish) return wrong(c, rung, now);
        if (now < pending->round_start + Milliseconds(pending->challenge.round.duration_ms)) {
            LadderOutcome early = wrong(c, rung, now);
            early.verdict = LadderVerdict::Early;
            return early;
        }
        if (now >= pending->expires) {
            LadderOutcome late = wrong(c, rung, now);
            late.verdict = LadderVerdict::Expired;
            return late;
        }
        return pending->hits >= pending->challenge.round.needed ? win(now) : wrong(c, rung, now);
    }
    if (rung != c.progress.rung) return result(LadderVerdict::Rejected, now);
    if (now >= pending->expires) return result(LadderVerdict::Expired, now);
    bool right = false;
    if (rung == LadderRung::DimSum) right = kind == Kind::Dish && choice == pending->answer;
    else if (rung == LadderRung::Sums)
        right = kind == Kind::Sums && answers != nullptr && answers->size() == ladder_sum_count &&
                std::equal(answers->begin(), answers->end(), pending->sums.begin());
    return right ? win(now) : wrong(c, rung, now);
}

LadderOutcome UnlockLadder::answer_dish(std::string_view nonce, unsigned choice, Time now)
{
    return grade(nonce, Kind::Dish, choice, nullptr, now);
}
LadderOutcome UnlockLadder::answer_sums(std::string_view nonce, const std::vector<long long>& answers, Time now)
{
    return grade(nonce, Kind::Sums, 0, &answers, now);
}
LadderOutcome UnlockLadder::finish_round(std::string_view nonce, Time now)
{
    return grade(nonce, Kind::Finish, 0, nullptr, now);
}

StrikeOutcome UnlockLadder::strike(std::string_view nonce, unsigned cell, Time now)
{
    StrikeOutcome outcome;
    if (!m_pending || !same_nonce(m_pending->challenge.nonce, nonce)) return outcome;
    if (m_pending->challenge.rung != LadderRung::Moles) {
        grade(nonce, Kind::Strike, 0, nullptr, now); // a strike is the wrong kind of answer here
        return outcome;
    }
    context(now);
    if (!m_pending) return outcome; // the wait ended or a new lockout began
    Pending& round = *m_pending;
    const MoleRound& schedule = round.challenge.round;
    outcome.hits = round.hits;
    outcome.strikes_left = schedule.strike_limit - round.strikes;
    const long long at = std::chrono::duration_cast<Milliseconds>(now - round.round_start).count();
    if (at < 0 || at >= static_cast<long long>(schedule.duration_ms)) {
        outcome.result = StrikeResult::OutsideRound;
        return outcome;
    }
    if (round.strikes >= schedule.strike_limit) return outcome; // tapping everything is not hitting moles
    ++round.strikes;
    outcome.strikes_left = schedule.strike_limit - round.strikes;
    outcome.result = StrikeResult::Empty;
    for (std::size_t i = 0; i < schedule.moles.size() && cell < ladder_mole_cells; ++i) {
        const Mole& mole = schedule.moles[i];
        if (mole.cell != cell || at < mole.appears_ms || at >= static_cast<long long>(mole.appears_ms) + mole.visible_ms) continue;
        if (round.hit[i]) outcome.result = StrikeResult::Repeat;
        else {
            round.hit[i] = true;
            ++round.hits;
            outcome.result = StrikeResult::Hit;
        }
        break;
    }
    outcome.hits = round.hits;
    return outcome;
}

} // namespace Slic3r::LocalSecurity
