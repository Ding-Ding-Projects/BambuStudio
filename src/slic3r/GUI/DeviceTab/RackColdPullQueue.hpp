#pragma once

#include <array>
#include <map>
#include <set>
#include <string>

namespace Slic3r::GUI::ColdPull {
enum class Step { Pick, Picking, StartPull, Pulling, Confirm, Return, Returning, Done, Stopped };

// Only the matching in-flight movement may temporarily have no settled identity
// at the source or destination. All user actions still require a stable reading.
inline bool expected_exchange(Step step, int slot, bool picking, bool placing,
                              bool switching, int reported_source, int reported_target)
{
    if (slot < 0 || slot >= 6 || (step != Step::Picking && step != Step::Returning)) return false;
    const bool taking = step == Step::Picking;
    if ((!switching && !(taking ? picking : placing)) || (taking ? placing : picking)) return false;
    const int position = 0x10 + slot;
    return (reported_source < 0 || reported_source == (taking ? position : 0)) &&
           (reported_target < 0 || reported_target == (taking ? 0 : position));
}

struct Reading {
    bool valid = false;
    bool idle = false;
    bool cold_pull = false;
    bool exchange = false;
    std::array<std::string, 6> dock;
    std::string head;
    int head_slot = -1;
};

// Commands originate only from explicit operator actions. Reports advance observations only.
class Queue {
public:
    Step step = Step::Stopped;
    int slot = -1;
    unsigned finished = 0;
    unsigned count = 0;

    void start(const Reading& r) {
        if (!r.valid || !r.idle) return;
        std::set<std::string> seen;
        for (int i = 0; i < 6; ++i) {
            auto serial = r.dock[i];
            if (!r.head.empty() && r.head_slot == i) {
                if (!serial.empty()) return;
                serial = r.head;
            }
            if (!serial.empty()) {
                if (!seen.insert(serial).second) return;
                targets[i] = serial;
            }
        }
        if (targets.empty() || (!r.head.empty() && !targets.count(r.head_slot))) return;
        count = static_cast<unsigned>(targets.size());
        slot = r.head.empty() ? targets.begin()->first : r.head_slot;
        step = r.head.empty() ? Step::Pick : Step::StartPull;
    }

    void observe(const Reading& r) {
        if (step == Step::Stopped || step == Step::Done) return;
        if (!r.valid) { step = Step::Stopped; return; }
        const bool moving = step == Step::Picking || step == Step::Returning;
        const bool servicing = step == Step::StartPull || step == Step::Pulling;
        if (!r.idle && !(moving && r.exchange) && !(servicing && r.cold_pull)) {
            step = Step::Stopped; return;
        }
        for (int i = 0; i < 6; ++i) {
            const auto target = targets.find(i);
            if (i != slot && r.dock[i] != (target == targets.end() ? "" : target->second)) {
                step = Step::Stopped; return;
            }
        }
        const auto& expected = targets.at(slot);
        if ((!r.dock[slot].empty() && r.dock[slot] != expected) ||
            (!r.head.empty() && (r.head != expected || r.head_slot != slot))) {
            step = Step::Stopped; return;
        }
        const bool in_dock = r.dock[slot] == expected && r.head.empty();
        const bool on_head = r.head == expected && r.dock[slot].empty();
        if (step == Step::Picking && r.idle && on_head) step = Step::StartPull;
        else if (step == Step::Returning && r.idle && in_dock) {
            completed.insert(slot);
            ++finished;
            if (finished == count) step = Step::Done;
            else for (const auto& target : targets) {
                if (!completed.count(target.first)) { slot = target.first; step = Step::Pick; break; }
            }
        } else if (!moving) {
            if (step == Step::Pick ? !in_dock : !on_head) { step = Step::Stopped; return; }
            if (step == Step::StartPull && r.cold_pull) step = Step::Pulling;
            else if (step == Step::Pulling && r.idle) step = Step::Confirm;
        }
    }

    void confirm() {
        if (step == Step::Pick) step = Step::Picking;
        else if (step == Step::Confirm) step = Step::Return;
        else if (step == Step::Return) step = Step::Returning;
    }

private:
    std::map<int, std::string> targets;
    std::set<int> completed;
};
}
