# Canvas menu search

The layer slider add and edit menus, each filament submenu, SVG file actions,
orientation method picker, ordinary canvas combo boxes, and filtered canvas combo boxes have local search.
The reachable Assembly Export popup uses the same header for its three export
formats and export-parameter action.
The search header provides literal matching by default, an explicit regular
expression toggle, the full anchored regex builder, and clear search. Narrow
popups place these controls on a second row and cap popup size to the viewport.

Each field is keyed by its ImGui ID in its owning ImGuiWrapper. At most 128 fields
are retained. Inactive fields are evicted oldest first, never while a worker is
running. Fields have independent query, flags, bridge callbacks and results.
No query or candidate is written to persistent storage or sent over a network.

Regex matching runs asynchronously through the existing bounded worker. Results
are cached by the complete query, flags and ordered labels. Pending or invalid
searches show all actions with explicit status; a valid empty result says
"No matches". Stale or incomplete results cannot hide a subset of actions.
Temporary worker startup or contention (`WorkerUnavailable`) retries after 250 ms
and then 1000 ms, with at most three total attempts for unchanged input. Invalid
patterns, complexity limits, timeouts and protocol errors do not retry. The
canvas schedules timed frames rather than continuously redrawing while waiting;
changing the query resets this bounded retry policy. Literal search uses bounded
in-process matching. Matching uses original localized
labels; optional display vocabulary changes only the rendered text and not IDs,
actions, saved data or logs.

Escape clears a nonempty search before a subsequent Escape closes the popup.
Holding Escape does not perform both operations. Opening a menu focuses its
search field. ImGui retains navigation and disabled-action behavior. Platform
screen-reader exposure still requires separate built-application verification;
visible result text alone is not evidence of an OS accessibility announcement.

Preview playback and layer slider colors resolve the same Brand palette as the
surrounding application, retaining theme roles and alpha rather than fixed colors.

## Required verification

All execution is assigned to GitHub-hosted Windows runners for this change.
No local product build, test, runtime or screenshot was executed.

- Build the application with the PersonalVocabulary implementation integrated.
- Compile and run `tests/canvas_menu/search_state_tests.cpp` using C++17 and the
  `src` include path. It exercises the actual header model used by rendering.
- Verify each listed root and submenu with empty, literal, regex, invalid,
  no-match, Unicode and whole-word inputs; preserve order and disabled states.
- Alternate controls and builder edits, close/reopen, then rapidly change queries
  during evaluation. Confirm no query or late result crosses control ownership.
- Verify keyboard opening, focus, arrows, Enter, clear, first Escape and second
  Escape. Check keyboard builder positioning at the activating canvas control.
- Capture English, Cantonese and bilingual menus in both themes at 100%, 125%,
  150% and 200% scales and minimum supported canvas dimensions. Check scrolling,
  popup bounds, long labels, theme consistency and platform accessibility.

These are pending verification requirements, not completed runtime evidence.

## Assembly Export popup

The Export button becomes enabled when the assembly contains at least one real
Normal or FinalAssembly step. Its popup searches the four original localized
labels in their original order: Export PDF, Export Markdown, Export MP4 and Set
export file parameters. The visibility mask never renumbers these actions.
PDF, Markdown and MP4 retain their original export callbacks; the final row
retains the export-settings dialog. The Markdown explanation remains available
on pointer hover or keyboard focus. Selecting a row still closes the popup.

Search ownership includes the AssemblyStepsUtils instance and popup identity.
The shared header supplies literal/regex matching, builder, clear and no-match
status. Rows participate in keyboard navigation. Popup height follows the
filtered content and search header, while the actual display bounds constrain
its size and permit scrolling when content cannot fit. The existing disabled
Export state and its guidance are unchanged.

Required hosted fixture: load a real model and create at least one actual
assembly step, then return to the overall assembly view where Export is enabled.
Verify each row's retained identity, literal and regex filtering, no matches,
clear, builder return, Down/Up navigation, Enter and Escape. Verify the empty
assembly's disabled button separately. Capture normal and minimum dimensions,
the supported languages/themes and measured display scales. Do not report an
export as completed from merely selecting or filtering a row. No local build,
test or UI execution was performed; compilation and this fixture remain pending.
