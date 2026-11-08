#include "LockModel.hpp"

#include "../Base64Toolkit/Base64Codec.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

namespace Slic3r { namespace GUI { namespace ElementLocks {

using json = nlohmann::json;

namespace {

std::string normalized(std::string_view s)
{
    std::string out;
    for (unsigned char c : s)
        if (!std::isspace(c)) out.push_back(char(std::tolower(c)));
    return out;
}

// Comparison that takes as long whatever the first differing character.
bool same(const std::string &a, const std::string &b)
{
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;
}

} // namespace

bool valid_code(std::string_view code, std::string &problem)
{
    problem.clear();
    std::size_t chars = 0;
    for (unsigned char c : code)
        if ((c & 0xC0) != 0x80) ++chars;
    if (code.find_first_of("\r\n") != std::string_view::npos) problem = "line-break";
    else if (chars < 4) problem = "short";
    else if (chars > 32) problem = "long";
    else if (std::all_of(code.begin(), code.end(), [](unsigned char c) { return std::isspace(c); })) problem = "blank";
    return problem.empty();
}

std::string hash_code(std::string_view code, const std::string &salt_hex)
{
    using Base64Toolkit::Sha256;
    Sha256 first;
    first.update(reinterpret_cast<const std::uint8_t *>(salt_hex.data()), salt_hex.size());
    first.update(reinterpret_cast<const std::uint8_t *>(code.data()), code.size());
    std::array<std::uint8_t, 32> digest = first.finish();
#ifndef SLIC3R_ELEMENT_LOCKS_NEGATIVE_FIXTURE
    for (int i = 1; i < kHashRounds; ++i) {
        Sha256 round;
        round.update(digest.data(), digest.size());
        round.update(reinterpret_cast<const std::uint8_t *>(salt_hex.data()), salt_hex.size());
        digest = round.finish();
    }
#endif
    return Base64Toolkit::hex(digest);
}

Lock make_lock(const std::string &element, const std::string &label, const std::string &reason, const std::string &who, std::string_view code,
               std::int64_t now, const std::string &salt_hex)
{
    Lock lock;
    lock.element = element;
    lock.label   = label.substr(0, 120);
    lock.reason  = reason.substr(0, 300);
    lock.who     = who.substr(0, 80);
    lock.created = now;
    lock.salt    = salt_hex;
    lock.hash    = hash_code(code, salt_hex);
    return lock;
}

std::int64_t lockout_after(int failures)
{
    if (failures < 3) return 0;
    switch (failures) {
    case 3: return 30;
    case 4: return 120;
    case 5: return 600;
    default: return 3600;
    }
}

Attempt try_code(Lock &lock, std::string_view code, std::int64_t now)
{
    if (now < lock.blocked_until) return Attempt::Blocked;
    if (same(hash_code(code, lock.salt), lock.hash)) {
        lock.failures      = 0;
        lock.blocked_until = 0;
        return Attempt::Opened;
    }
    ++lock.failures;
    lock.blocked_until = now + lockout_after(lock.failures);
    return Attempt::Wrong;
}

const std::vector<LadderStep> &ladder()
{
    static const std::vector<LadderStep> steps = {
        {LadderStep::Kind::Wait, 60},
        {LadderStep::Kind::TypeName, 0},
        {LadderStep::Kind::Wait, 300},
    };
    return steps;
}

LadderState ladder_state(const Lock &lock, std::int64_t now, std::int64_t &seconds_left)
{
    seconds_left = 0;
    if (lock.ladder_started == 0) return LadderState::NotStarted;
    if (lock.ladder_step >= int(ladder().size())) return LadderState::Done;
    const LadderStep &step = ladder()[std::size_t(lock.ladder_step)];
    if (step.kind == LadderStep::Kind::TypeName) return LadderState::NeedsName;
    seconds_left = std::max<std::int64_t>(0, lock.step_started + step.seconds - now);
    return LadderState::Waiting;
}

void ladder_start(Lock &lock, std::int64_t now)
{
    lock.ladder_started = now;
    lock.ladder_step    = 0;
    lock.step_started   = now;
}

bool ladder_advance(Lock &lock, std::int64_t now, std::string_view typed_name)
{
    if (lock.ladder_started == 0 || lock.ladder_step >= int(ladder().size())) return false;
    const LadderStep &step = ladder()[std::size_t(lock.ladder_step)];
    if (step.kind == LadderStep::Kind::Wait) {
        if (now < lock.step_started + step.seconds) return false;
    } else if (normalized(typed_name).empty() || normalized(typed_name) != normalized(lock.label)) {
        return false;
    }
    ++lock.ladder_step;
    lock.step_started = now;
    return true;
}

Lock *Store::find(const std::string &element)
{
    const auto it = m_locks.find(element);
    return it == m_locks.end() ? nullptr : &it->second;
}

const Lock *Store::find(const std::string &element) const
{
    const auto it = m_locks.find(element);
    return it == m_locks.end() ? nullptr : &it->second;
}

bool Store::load(std::string_view text)
{
    m_locks.clear();
    const json j = json::parse(text.begin(), text.end(), nullptr, false);
    if (j.is_discarded() || !j.is_object() || !j.contains("locks") || !j["locks"].is_array()) return false;
    for (const json &l : j["locks"]) {
        if (!l.is_object()) continue;
        Lock lock;
        const auto str = [&l](const char *k) { return l.contains(k) && l[k].is_string() ? l[k].get<std::string>() : std::string(); };
        const auto num = [&l](const char *k) { return l.contains(k) && l[k].is_number_integer() ? l[k].get<std::int64_t>() : std::int64_t(0); };
        lock.element        = str("element");
        lock.label          = str("label");
        lock.reason         = str("reason");
        lock.who            = str("who");
        lock.salt           = str("salt");
        lock.hash           = str("hash");
        lock.created        = num("created");
        lock.failures       = int(num("failures"));
        lock.blocked_until  = num("blockedUntil");
        lock.ladder_started = num("ladderStarted");
        lock.ladder_step    = int(num("ladderStep"));
        lock.step_started   = num("stepStarted");
        // A lock without its hash cannot be opened with a code, so it is not kept.
        if (lock.element.empty() || lock.salt.size() < 16 || lock.hash.size() != 64) continue;
        m_locks[lock.element] = lock;
    }
    return true;
}

std::string Store::save() const
{
    json list = json::array();
    for (const auto &[id, l] : m_locks)
        list.push_back(json{{"element", l.element}, {"label", l.label}, {"reason", l.reason}, {"who", l.who}, {"created", l.created},
                            {"salt", l.salt}, {"hash", l.hash}, {"failures", l.failures}, {"blockedUntil", l.blocked_until},
                            {"ladderStarted", l.ladder_started}, {"ladderStep", l.ladder_step}, {"stepStarted", l.step_started}});
    return json{{"version", 1}, {"locks", list}}.dump(1);
}

}}} // namespace Slic3r::GUI::ElementLocks
