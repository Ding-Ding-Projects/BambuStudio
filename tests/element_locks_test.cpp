// Standalone assertions for src/slic3r/GUI/ElementLocks/LockModel.cpp: codes
// that are refused, the salted, iterated hash (checked against an independent
// computation), wrong codes and the waits after them, the unlock ladder, and
// saving and loading. Compiled and run by tests/element_locks.test.mjs.
#include "src/slic3r/GUI/Base64Toolkit/Base64Codec.cpp"
#include "src/slic3r/GUI/ElementLocks/LockModel.cpp"

#include <cstdio>

using namespace Slic3r::GUI::ElementLocks;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

int main()
{
    std::string problem;
    CHECK(!valid_code("123", problem) && problem == "short");
    CHECK(!valid_code("    ", problem) && problem == "blank");
    CHECK(!valid_code("12\n34", problem) && problem == "line-break");
    CHECK(!valid_code(std::string(33, '7'), problem) && problem == "long");
    CHECK(valid_code("2468", problem) && valid_code("\xE5\xAF\x86\xE7\xA2\xBC\xE5\xAF\x86\xE7\xA2\xBC", problem)); // four characters

    // 20,000 rounds of SHA-256 over the salt, computed independently.
    const std::string salt = "00112233445566778899aabbccddeeff";
    CHECK(hash_code("2468", salt) == "7112782f8de5f892f4b6c404fff58df918e30e4794b823ad24743d4693a1abe2");

    const std::int64_t t0   = 1762581600;
    Lock               lock = make_lock("prefs.funny-level", "Funny level", "Exams this week", "swift", "2468", t0, salt);
    CHECK(lock.hash.size() == 64 && lock.hash != "2468" && lock.who == "swift");

    // Two wrong codes cost nothing, the third blocks for 30 s, and blocked tries are not even compared.
    CHECK(try_code(lock, "0000", t0) == Attempt::Wrong && lock.blocked_until == t0);
    CHECK(try_code(lock, "1111", t0 + 1) == Attempt::Wrong && lock.blocked_until == t0 + 1);
    CHECK(try_code(lock, "2222", t0 + 2) == Attempt::Wrong && lock.blocked_until == t0 + 32);
    CHECK(lock.failures == kLadderAfter);
    CHECK(try_code(lock, "2468", t0 + 10) == Attempt::Blocked && lock.failures == 3);
    CHECK(try_code(lock, "3333", t0 + 40) == Attempt::Wrong && lock.blocked_until == t0 + 160);
    CHECK(lockout_after(5) == 600 && lockout_after(9) == 3600 && lockout_after(2) == 0);
    CHECK(try_code(lock, "2468", t0 + 200) == Attempt::Opened && lock.failures == 0 && lock.blocked_until == 0);

    // The ladder: wait a minute, type the name, wait five more.
    std::int64_t left = 0;
    CHECK(ladder_state(lock, t0, left) == LadderState::NotStarted);
    ladder_start(lock, t0 + 1000);
    CHECK(ladder_state(lock, t0 + 1030, left) == LadderState::Waiting && left == 30);
    CHECK(!ladder_advance(lock, t0 + 1030));
    CHECK(ladder_advance(lock, t0 + 1060));
    CHECK(ladder_state(lock, t0 + 1061, left) == LadderState::NeedsName);
    CHECK(!ladder_advance(lock, t0 + 1062, "Funny levels"));
    CHECK(!ladder_advance(lock, t0 + 1062, ""));
    CHECK(ladder_advance(lock, t0 + 1063, "  funny LEVEL "));
    CHECK(ladder_state(lock, t0 + 1100, left) == LadderState::Waiting && left == 263);
    CHECK(!ladder_advance(lock, t0 + 1300));
    CHECK(ladder_advance(lock, t0 + 1363));
    CHECK(ladder_state(lock, t0 + 1364, left) == LadderState::Done);
    CHECK(!ladder_advance(lock, t0 + 1365));

    // Saving and loading; a lock without its hash is dropped.
    Store store;
    store.put(lock);
    Store back;
    CHECK(back.load(store.save()) && back.find("prefs.funny-level") && back.find("prefs.funny-level")->hash == lock.hash);
    CHECK(back.find("prefs.funny-level")->ladder_step == lock.ladder_step);
    CHECK(back.load(R"({"version":1,"locks":[{"element":"x.y","salt":"00112233445566778899aabbccddeeff","hash":"short"}]})") && back.empty());
    CHECK(!back.load("not json") && !back.load("[]"));
    CHECK(store.remove("prefs.funny-level") && store.empty());

    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("element locks: all checks passed\n");
    return 0;
}
