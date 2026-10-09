#pragma once
#include "LocalSecurity.hpp"

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The unlock ladder: something to do during a lockout wait instead of watching
// a countdown. A dim-sum question, then ten easy sums, then whack-a-mole, then
// the clock. This service owns every question and every grade. A surface only
// renders what `challenge()` returns and hands answers back with the nonce; it
// never sees an expected answer and never decides a result.
//
// Winning clears the WAIT and nothing else. It never signs anybody in, never
// unlocks a LockSession, restores exactly the attempts that ordinary expiry
// restores, leaves the escalation untouched, and is capped per rolling hour.
namespace Slic3r::LocalSecurity {

// One public-catalog dish the dim-sum rung may use. Names and alt text are the
// catalog's own facts, carried through unchanged.
struct LadderDish {
    std::string id;
    std::string name_en;
    std::string name_zh_hant;
    std::string alt_en;
    std::string alt_yue;
    std::string image_file;   // catalog photo basename
    bool photo_ready = false; // the photo is already on this machine
};

// Shipped rules. Changing any of these changes the feature contract.
inline constexpr unsigned ladder_dish_choices = 4;
inline constexpr unsigned ladder_wrong_dishes = 5;      // wrong dishes before the sums
inline constexpr unsigned ladder_sum_count = 10;
inline constexpr unsigned ladder_mole_columns = 3;
inline constexpr unsigned ladder_mole_rows = 3;
inline constexpr unsigned ladder_mole_cells = ladder_mole_columns * ladder_mole_rows;
inline constexpr unsigned ladder_mole_count = 10;
inline constexpr unsigned ladder_moles_needed = 6;
inline constexpr unsigned ladder_mole_visible_ms = 2000;
inline constexpr unsigned ladder_mole_spacing_ms = 2400; // moles never overlap
inline constexpr unsigned ladder_round_lead_in_ms = 3000;
inline constexpr unsigned ladder_round_ms = ladder_mole_count * ladder_mole_spacing_ms;
inline constexpr unsigned ladder_strike_limit = 2 * ladder_mole_count;
inline constexpr unsigned ladder_dish_seconds = 120;
inline constexpr unsigned ladder_sums_seconds = 300;
inline constexpr unsigned ladder_round_grace_ms = 15000;

// True when the catalog data can make a fair dim-sum question: one dish whose
// photo is on this machine and three other dishes with different names.
bool dim_sum_rung_ready(const std::vector<LadderDish>& dishes);
// The one decision of where a lockout's ladder starts. Under School mode the
// dim-sum rung is absent, so the ladder starts at the sums. Without catalog
// data for a fair question the ladder also starts at the sums.
LadderRung ladder_start(bool school_mode, bool dim_sum_ready) noexcept;

struct DishChoice {
    std::string name_en;
    std::string name_zh_hant;
};
struct DishQuestion {
    std::string image_file;
    std::string alt_en;
    std::string alt_yue;
    std::array<DishChoice, ladder_dish_choices> choices;
};
struct SumProblem {
    unsigned left = 0;
    char operation = '+'; // '+' or '-'
    unsigned right = 0;
};
// Times are milliseconds after the round starts.
struct Mole {
    unsigned cell = 0; // row-major, 0 is top left
    unsigned appears_ms = 0;
    unsigned visible_ms = 0;
};
struct MoleRound {
    unsigned lead_in_ms = 0; // the round starts this long after it is issued
    unsigned duration_ms = 0;
    unsigned needed = 0;
    unsigned strike_limit = 0;
    std::vector<Mole> moles;
};
// What a surface may show. It never contains an expected answer.
struct LadderChallenge {
    std::string nonce; // single use, 32 lowercase hexadecimal characters
    LadderRung rung = LadderRung::Clock;
    unsigned expires_in_seconds = 0;
    DishQuestion dish;                              // rung == DimSum
    std::array<SumProblem, ladder_sum_count> sums{}; // rung == Sums
    MoleRound round;                                // rung == Moles
};

struct LadderStatus {
    bool waiting = false;          // a lockout wait is running
    unsigned wait_seconds = 0;
    unsigned remaining_attempts = 0;
    LadderRung rung = LadderRung::Clock; // Clock: only the wait remains
    unsigned wrong_dishes = 0;
    unsigned skips_left = 0;       // ladder wins still allowed this rolling hour
    bool offered = false;          // waiting, a rung to play, and allowance left
};
enum class LadderVerdict {
    Cleared,    // the wait is over; signing in still needs the credential
    Wrong,      // graded wrong; the status says where the ladder now stands
    Early,      // a timed round handed in before it lasted; graded as lost
    Expired,    // the challenge ran out of time
    Rejected,   // an unknown, used or withdrawn nonce; nothing was graded
    NotLocked,  // no wait is running any more
    CapReached, // the hourly allowance is spent; only the clock remains
};
struct LadderOutcome {
    LadderVerdict verdict = LadderVerdict::Rejected;
    LadderStatus status;
};
enum class StrikeResult {
    Hit,          // a visible mole in that cell, hit for the first time
    Empty,        // nothing visible in that cell
    Repeat,       // that mole was already hit
    OutsideRound, // before the round started or after it ended
    Rejected,     // unknown nonce, wrong kind of challenge, or strike limit reached
};
struct StrikeOutcome {
    StrikeResult result = StrikeResult::Rejected;
    unsigned hits = 0;
    unsigned strikes_left = 0;
};

// One ladder per lockout surface, sharing that surface's AttemptBudget and the
// one LadderAllowance every surface spends. The budget keeps the ladder's
// position with the lockout it belongs to, so a persisted budget carries it
// across restarts. Not thread-safe: the surface that owns the lockout owns its
// ladder.
class UnlockLadder {
public:
    // `school_mode` is read at every decision; it must be callable.
    UnlockLadder(AttemptBudget& budget, LadderAllowance& allowance, std::function<bool()> school_mode,
                 std::vector<LadderDish> dishes);
    ~UnlockLadder();
    UnlockLadder(const UnlockLadder&) = delete;
    UnlockLadder& operator=(const UnlockLadder&) = delete;

    void set_dishes(std::vector<LadderDish> dishes);
    LadderStatus status(Time now);
    // The live challenge on the current rung. Asking again returns the same
    // challenge until it is answered or expires, so a question cannot be
    // re-rolled. Empty when nothing is waiting, the ladder fell to the clock,
    // or the hourly allowance is spent.
    std::optional<LadderChallenge> challenge(Time now);
    // Every answer consumes its nonce before grading. Handing an answer of the
    // wrong kind to a challenge grades that challenge as wrong.
    LadderOutcome answer_dish(std::string_view nonce, unsigned choice, Time now);
    LadderOutcome answer_sums(std::string_view nonce, const std::vector<long long>& answers, Time now);
    // Graded the moment it arrives, by this service's clock.
    StrikeOutcome strike(std::string_view nonce, unsigned cell, Time now);
    // Rejected as early before the round has lasted its full duration.
    LadderOutcome finish_round(std::string_view nonce, Time now);

private:
    struct Pending;
    struct Context {
        AttemptState attempts;
        LadderProgress progress;
        bool waiting = false;
    };
    enum class Kind { Dish, Sums, Strike, Finish };
    Context context(Time now);
    LadderStatus make_status(const Context&, Time now);
    LadderOutcome result(LadderVerdict, Time now);
    LadderOutcome grade(std::string_view nonce, Kind, unsigned choice, const std::vector<long long>* answers, Time now);
    LadderOutcome wrong(const Context&, LadderRung pending, Time now);
    LadderOutcome win(Time now);
    LadderChallenge issue(const Context&, Time now);

    AttemptBudget& m_budget;
    LadderAllowance& m_allowance;
    std::function<bool()> m_school_mode;
    std::vector<LadderDish> m_dishes;
    bool m_dim_sum_ready = false;
    std::unique_ptr<Pending> m_pending;
};

} // namespace Slic3r::LocalSecurity
