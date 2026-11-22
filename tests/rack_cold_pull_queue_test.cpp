#include "../src/slic3r/GUI/DeviceTab/RackColdPullQueue.hpp"
#include <cassert>

using namespace Slic3r::GUI::ColdPull;

int main()
{
    assert(expected_exchange(Step::Picking, 4, true, false, false, 20, 0));
    assert(expected_exchange(Step::Returning, 4, false, true, false, 0, 20));
    assert(expected_exchange(Step::Picking, 4, false, false, true, 20, 0));
    assert(!expected_exchange(Step::Pick, 4, true, false, true, 20, 0));
    assert(!expected_exchange(Step::Picking, 4, true, false, true, 19, 0));
    assert(!expected_exchange(Step::Picking, 4, false, true, true, 20, 0));
    assert(!expected_exchange(Step::Picking, 4, false, false, false, -1, -1));
    Reading base;
    base.valid = base.idle = true;
    base.dock[0] = "A"; base.dock[4] = "B";
    Queue queue;
    queue.start(base);
    assert(queue.step == Step::Pick && queue.slot == 0 && queue.count == 2);
    for (int slot : {0,4}) {
        auto r = base;
        queue.confirm();
        assert(queue.step == Step::Picking);
        queue.observe(r);
        assert(queue.step == Step::Picking);
        auto in_transit = r;
        in_transit.idle = false;
        in_transit.exchange = true;
        in_transit.dock[slot].clear();
        queue.observe(in_transit);
        assert(queue.step == Step::Picking && queue.finished < queue.count);
        r.head = r.dock[slot]; r.head_slot = slot; r.dock[slot].clear();
        queue.observe(r);
        assert(queue.step == Step::StartPull);
        queue.confirm(); queue.observe(r);
        assert(queue.step == Step::StartPull);
        r.idle = false; r.cold_pull = true;
        queue.observe(r);
        assert(queue.step == Step::Pulling);
        r.idle = true; r.cold_pull = false;
        queue.observe(r);
        assert(queue.step == Step::Confirm);
        queue.observe(r);
        assert(queue.step == Step::Confirm);
        queue.confirm();
        assert(queue.step == Step::Return);
        queue.confirm();
        queue.observe(r);
        assert(queue.step == Step::Returning);
        queue.observe(base);
    }
    assert(queue.step == Step::Done && queue.finished == 2);

    for (int i = 0; i < 6; ++i) {
        Queue q;
        q.start(base);
        auto r = base;
        if (i == 0) r.valid = false;
        if (i == 1) r.dock[4] = "C";
        if (i == 2) r.dock[0].clear();
        if (i == 3) r.idle = false;
        if (i == 4) { r.head = "A"; r.head_slot = 4; }
        if (i == 5) r.dock[2] = "C";
        q.observe(r); q.confirm(); q.observe(base);
        assert(q.step == Step::Stopped && q.finished == 0);
    }
    auto r = base;
    r.dock[4] = "A";
    Queue duplicate;
    duplicate.start(r);
    assert(duplicate.step == Step::Stopped);
    r = base; r.head = "B"; r.head_slot = 4; r.dock[4].clear();
    Queue mounted;
    mounted.start(r);
    assert(mounted.step == Step::StartPull && mounted.slot == 4);
    mounted.observe(r);
    assert(mounted.step == Step::StartPull);
}
