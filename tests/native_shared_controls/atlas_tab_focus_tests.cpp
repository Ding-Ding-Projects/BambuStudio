#include <algorithm>
#include <iostream>
#include <vector>
#include "atlas_tab_overflow.inc"
#include "atlas_tab_focus.inc"

std::vector<int> projection(int width)
{
    const auto allocation = atlasTabOverflow({100, 400, 100, 100}, {true, true, true, true}, width, 36, 4);
    std::vector<bool> shown(4, false);
    for (int index : allocation.visible) shown[index] = true;
    return atlasFocusTargets({0, 1, 2, 3}, shown, allocation.needs_button);
}

int main()
{
    int assertions = 0, failures = 0;
    auto verify = [&](bool condition) { ++assertions; if (!condition) ++failures; };
    const auto wide = projection(800);
    const auto narrow = projection(400);
    verify(wide == std::vector<int>({0, 1, 2, 3}));
    verify(narrow == std::vector<int>({0, 2, 3, atlasOverflowFocus}));

    // Resizing away the focused long pinned tab gives focus to the real overflow target.
    int focused = atlasReconcileFocus(1, 0, narrow);
    verify(focused == atlasOverflowFocus);
    verify(atlasFocusActivation(focused, narrow) == AtlasFocusAction::OpenOverflow);

    // End then Enter reaches overflow, rather than activating an invisible tab.
    focused = narrow.back();
    verify(focused == atlasOverflowFocus);
    verify(atlasFocusActivation(focused, narrow) == AtlasFocusAction::OpenOverflow);
    focused = atlasReconcileFocus(focused, 0, narrow); // cancel returns to the same visible target
    verify(focused == atlasOverflowFocus);

    // Arrows wrap across only visible controls; Home returns to the first tab.
    focused = narrow.front();
    for (int expected : {2, 3, atlasOverflowFocus, 0}) {
        focused = atlasStepFocus(focused, narrow, 1);
        verify(focused == expected);
    }
    verify(atlasStepFocus(0, narrow, -1) == atlasOverflowFocus);

    // Menu activation of a visible destination returns to that tab, while a still
    // overflowed destination returns to overflow. Neither route changes model order.
    verify(atlasReconcileFocus(2, 2, narrow) == 2);
    verify(atlasFocusActivation(2, narrow) == AtlasFocusAction::ActivateTab);
    verify(atlasReconcileFocus(1, 1, narrow) == atlasOverflowFocus);

    // Growing away overflow restores a visible active destination.
    verify(atlasReconcileFocus(atlasOverflowFocus, 2, wide) == 2);
    verify(atlasFocusActivation(2, wide) == AtlasFocusAction::ActivateTab);

    // An all-overflow strip still has one keyboard target and an opening action.
    const auto all_overflow = projection(40);
    verify(all_overflow == std::vector<int>({atlasOverflowFocus}));
    focused = atlasReconcileFocus(3, 3, all_overflow);
    verify(focused == atlasOverflowFocus);
    verify(atlasStepFocus(focused, all_overflow, 1) == atlasOverflowFocus);
    verify(atlasStepFocus(focused, all_overflow, -1) == atlasOverflowFocus);
    verify(all_overflow.front() == all_overflow.back());
    verify(atlasFocusActivation(focused, all_overflow) == AtlasFocusAction::OpenOverflow);
    verify(atlasReconcileFocus(focused, 3, all_overflow) == atlasOverflowFocus);

    // Missing projections and stale identities never activate hidden destinations.
    verify(atlasReconcileFocus(1, 0, {}) == -1);
    verify(atlasStepFocus(-1, {}, 1) == -1);
    verify(atlasFocusActivation(1, narrow) == AtlasFocusAction::None);
    verify(atlasFocusActivation(-1, {}) == AtlasFocusAction::None);
    verify(atlasFocusTargets({0, 2}, {true, true, false}, false) == std::vector<int>({0}));

    std::cout << "7 focus-projection cases; " << assertions << " assertions; " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
