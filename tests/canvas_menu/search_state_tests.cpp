// Standalone hosted test: cl /std:c++17 /EHsc /I src tests/canvas_menu/search_state_tests.cpp
#include "slic3r/GUI/CanvasMenuSearchModel.hpp"
#include <cstdlib>
#include <iostream>

using namespace Slic3r::GUI;
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
    first.accept_result(a, {true, false, true}, false);
    require(!first.needs_result() && !first.visible[1], "completed mask retains candidate order");
    second.change_input(plain, 3);
    require(second.visible[1] && second.needs_result(), "a second control does not inherit the first mask");
    first.change_input(plain, 3);
    require(!first.accept_result(a, {false, false, false}, false), "late regex result cannot overwrite literal mode");
    require(first.visible[0], "late result leaves current rows visible");
    first.change_input(a, 3);
    require(first.needs_result(), "returning to a prior query requires a fresh result after invalidation");
    first.accept_result(a, {false, true, false}, true);
    require(first.unavailable && first.visible == std::vector<bool>({true,true,true}), "incomplete pass never partially hides actions");
    first.accept_result(a, {false}, false);
    require(first.unavailable && first.visible.size() == 3, "wrong-size result cannot corrupt row indices");
    first.accept_result(a, {false, false, false}, false);
    require(!first.unavailable && first.visible == std::vector<bool>({false,false,false}), "valid no-match result remains distinguishable from unavailable");
    require(!first.change_input(a, 3) && !first.needs_result(), "unchanged frames do not repeat evaluation");
    std::cout << "PASS: " << checks << " canvas menu state checks\n";
}
