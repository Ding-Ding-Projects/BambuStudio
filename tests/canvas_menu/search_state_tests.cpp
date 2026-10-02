// Standalone hosted test: cl /std:c++17 /EHsc /I src tests/canvas_menu/search_state_tests.cpp
#include "slic3r/GUI/CanvasMenuSearchModel.hpp"
#include <cstdlib>
#include <iostream>

using namespace Slic3r::GUI;
using Status = BoundedRegex::Status;
static int checks = 0;
static void require(bool value, const char* name) {
    ++checks;
    if (!value) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); }
}
int main() {
    RegexBuilderValues query;
    query.pattern = "a";
    query.regex_enabled = true;
    const std::vector<std::string> items{"alpha", "beta", "gamma"};
    const auto a = canvas_menu_search_key(query, items);
    auto literal = query; literal.regex_enabled = false;
    const auto plain = canvas_menu_search_key(literal, items);
    require(a != plain, "literal and regex modes have independent result keys");
    auto other = query; other.whole_word = true;
    require(a != canvas_menu_search_key(other, items), "whole-word changes invalidate results");
    other = query; other.multiline = true;
    require(a != canvas_menu_search_key(other, items), "multiline changes invalidate results");
    other = query; other.case_sensitive = true;
    require(a != canvas_menu_search_key(other, items), "case changes invalidate results");
    require(canvas_menu_search_key(query, {"ab", "c"}) != canvas_menu_search_key(query, {"a", "bc"}), "candidate boundaries cannot alias");
    require(a != canvas_menu_search_key(query, {"beta", "alpha", "gamma"}), "candidate order belongs to identity");
    CanvasMenuSearchModel first, second;
    require(first.change_input(a, 3), "first input invalidates empty state");
    require(first.needs_result(), "new input requests evaluation");
    first.accept_result(a, {true, false, true}, Status::Valid);
    require(!first.needs_result() && !first.visible[1], "completed mask retains candidate order");
    second.change_input(plain, 3);
    require(second.visible[1] && second.needs_result(), "a second control does not inherit the first mask");
    first.change_input(plain, 3);
    require(!first.accept_result(a, {false, false, false}, Status::Valid), "late regex result cannot overwrite literal mode");
    require(first.visible[0], "late result leaves current rows visible");
    first.change_input(a, 3);
    require(first.needs_result(), "returning to a prior query requires a fresh result after invalidation");
    first.accept_result(a, {false, true, false}, Status::TimedOut);
    require(first.unavailable && first.visible == std::vector<bool>({true,true,true}), "incomplete pass never partially hides actions");
    first.accept_result(a, {false}, Status::Valid);
    require(first.unavailable && first.visible.size() == 3, "wrong-size result cannot corrupt row indices");
    first.accept_result(a, {false, false, false}, Status::Valid);
    require(!first.unavailable && first.visible == std::vector<bool>({false,false,false}), "valid no-match result remains distinguishable from unavailable");
    require(!first.change_input(a, 3) && !first.needs_result(), "unchanged frames do not repeat evaluation");
    const CanvasMenuSearchModel::TimePoint epoch{};
    using Ms = std::chrono::milliseconds;
    CanvasMenuSearchModel transient;
    transient.change_input(a, 3);
    require(transient.begin_request(epoch), "initial request starts immediately");
    transient.accept_result(a, {false, false, false}, Status::WorkerUnavailable, epoch);
    require(transient.unavailable && transient.visible == std::vector<bool>({true,true,true}), "transient unavailability keeps every action visible");
    require(transient.retry_waiting && transient.retry_delay_ms(epoch) == 250, "first retry waits 250 ms");
    require(!transient.needs_result(epoch + Ms(249)), "no evaluation during the first backoff");
    require(!transient.begin_request(epoch + Ms(249)) && transient.attempts == 1, "early dispatch cannot consume an attempt");
    require(transient.begin_request(epoch + Ms(250)) && transient.attempts == 2, "second attempt starts only at its deadline");
    transient.accept_result(a, {true, true, true}, Status::WorkerUnavailable, epoch + Ms(250));
    require(transient.retry_delay_ms(epoch + Ms(250)) == 1000, "second retry backs off for 1000 ms");
    require(!transient.needs_result(epoch + Ms(1249)), "no evaluation during longer backoff");
    require(transient.begin_request(epoch + Ms(1250)) && transient.attempts == 3, "third attempt starts at the longer deadline");
    transient.accept_result(a, {true, true, true}, Status::WorkerUnavailable, epoch + Ms(1250));
    require(!transient.retry_waiting && !transient.needs_result(epoch + Ms(100000)), "unchanged query stops after three total attempts");
    require(transient.retry_delay_ms(epoch + Ms(100000)) == 0, "exhausted retries schedule no frames");
    transient.change_input(plain, 3);
    require(transient.attempts == 0 && !transient.retry_waiting, "changed input resets retry state");
    require(!transient.accept_result(a, {false,false,false}, Status::WorkerUnavailable, epoch), "stale transient result is rejected");
    require(!transient.retry_waiting && transient.attempts == 0, "stale result cannot schedule a retry for new input");

    CanvasMenuSearchModel recovery;
    recovery.change_input(a, 3);
    recovery.begin_request(epoch);
    recovery.accept_result(a, {true,true,true}, Status::WorkerUnavailable, epoch);
    recovery.begin_request(epoch + Ms(250));
    recovery.accept_result(a, {true,false,true}, Status::Valid, epoch + Ms(250));
    require(!recovery.unavailable && !recovery.retry_waiting && !recovery.visible[1], "recovered worker applies the ordered result");
    require(!recovery.needs_result(epoch + Ms(100000)), "successful recovery remains cached");

    for (Status terminal : {Status::InvalidPattern, Status::PatternTooLong, Status::PatternTooComplex,
                            Status::TimedOut, Status::ProtocolError, Status::SubjectTooLong}) {
        CanvasMenuSearchModel failure;
        failure.change_input(a, 3);
        failure.begin_request(epoch);
        failure.accept_result(a, {false,false,false}, terminal, epoch);
        require(failure.unavailable && failure.visible == std::vector<bool>({true,true,true}), "terminal failure keeps all actions visible");
        require(!failure.retry_waiting && !failure.needs_result(epoch + Ms(100000)), "invalid and budget results never retry automatically");
    }
    CanvasMenuSearchModel malformed;
    malformed.change_input(a, 3);
    malformed.begin_request(epoch);
    malformed.accept_result(a, {true}, Status::WorkerUnavailable, epoch);
    require(!malformed.retry_waiting && malformed.unavailable, "malformed transient result is terminal");
    std::cout << "PASS: " << checks << " canvas menu state checks\n";
}
