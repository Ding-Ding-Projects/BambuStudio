#ifndef slic3r_GUI_ElementLocks_LockModel_hpp_
#define slic3r_GUI_ElementLocks_LockModel_hpp_

// Element locks, without wxWidgets: a lock on one element of the window (any
// control adopted under an element id), the code that opens it (kept only as
// a salted, iterated SHA-256), the waits that follow wrong codes, and the
// unlock ladder for a forgotten code: wait, type the element's name, wait
// longer, and the lock comes off. A lock guards against slips and impulses;
// it is not a security boundary, since anyone who can edit the settings file
// can remove it. Tested on its own by tests/element_locks_test.cpp.

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Slic3r { namespace GUI { namespace ElementLocks {

struct Lock
{
    std::string  element;        // the element id
    std::string  label;          // what the person saw when they locked it
    std::string  reason;         // optional, shown to whoever meets the lock
    std::string  who;            // the Windows user name
    std::int64_t created = 0;    // seconds since 1970-01-01 UTC
    std::string  salt;           // hex
    std::string  hash;           // hex
    int          failures = 0;   // wrong codes in a row
    std::int64_t blocked_until = 0;
    std::int64_t ladder_started = 0; // 0: not on the ladder
    int          ladder_step    = 0; // index into ladder()
    std::int64_t step_started   = 0;
};

constexpr int kHashRounds = 20000;

// 4 to 32 characters, no line breaks, not only spaces. problem: short, long, blank, line-break.
bool        valid_code(std::string_view code, std::string &problem);
std::string hash_code(std::string_view code, const std::string &salt_hex);
Lock        make_lock(const std::string &element, const std::string &label, const std::string &reason, const std::string &who,
                      std::string_view code, std::int64_t now, const std::string &salt_hex);

// Seconds a wrong code blocks the next try: none for the first two in a row,
// then 30 s, 2 min, 10 min and 1 hour from the sixth on.
std::int64_t lockout_after(int failures);

enum class Attempt { Opened, Wrong, Blocked };
// Blocked while blocked_until is in the future: the code is not even compared.
Attempt try_code(Lock &lock, std::string_view code, std::int64_t now);
// The ladder is offered after this many wrong codes in a row.
constexpr int kLadderAfter = 3;

struct LadderStep
{
    enum class Kind { Wait, TypeName };
    Kind kind    = Kind::Wait;
    int  seconds = 0;
};
const std::vector<LadderStep> &ladder();

enum class LadderState { NotStarted, Waiting, NeedsName, Done };
LadderState ladder_state(const Lock &lock, std::int64_t now, std::int64_t &seconds_left);
void        ladder_start(Lock &lock, std::int64_t now);
// Advances a finished wait; the name step needs the element's label typed exactly (case and spaces ignored).
bool        ladder_advance(Lock &lock, std::int64_t now, std::string_view typed_name = {});

class Store
{
public:
    bool                                load(std::string_view json);
    std::string                         save() const;
    const std::map<std::string, Lock>  &locks() const { return m_locks; }
    Lock                               *find(const std::string &element);
    const Lock                         *find(const std::string &element) const;
    void                                put(const Lock &lock) { m_locks[lock.element] = lock; }
    bool                                remove(const std::string &element) { return m_locks.erase(element) > 0; }
    bool                                empty() const { return m_locks.empty(); }

private:
    std::map<std::string, Lock> m_locks;
};

}}} // namespace Slic3r::GUI::ElementLocks

#endif // slic3r_GUI_ElementLocks_LockModel_hpp_
