# Prepare sidebar navigation handoff

## Scope and source

The Prepare sidebar groups its existing controls into three tabs: Ink, Process, and Objects. This responds to the reported narrow, vertically crowded sidebar. The supplied image shows an earlier running build and has no executable hash, so it is a problem reference, not after-change evidence.

The current source always uses the full process settings tree. The earlier compact Process card seen in the supplied image is not a claim about the current candidate. This change keeps the existing tree and its search, along with the existing filament and object searches.

Material Designer is not available as a callable design tool in this session. The last documented native trial in `design/README.md` rendered a black window and exposed no usable creation or export flow. The implementation therefore uses the application's existing Material Design 3 tab strip, with the actual wxWidgets controls as the source of truth.

## States

| Tab | Visible content | Preserved behavior |
| --- | --- | --- |
| Ink | Printer, bed, filament, AMS and material controls | Existing filament search and its regex builder, preset and slot state |
| Process | Settings search and full print settings tree | Existing settings search and its regex builder, preset selection, option jumps |
| Objects | Object search, plate/object list and manipulation card when selection is valid | Existing object search and its regex builder, expanded rows, selection and manipulation values |

The tab strip docks to any edge, defaults to the left edge and saves its layout through `TabStrip`. Its vertical rail is 128 DIP in Prepare, rather than the 230 DIP settings rail, so the content keeps usable width. Each tab uses the same scroll area, starting at the top on activation. Accessible tab names and orientation-aware keyboard navigation come from `TabStrip`. At narrow widths, the strip's overflow control must expose every tab that does not fit.

## Verification still required

Build the exact source revision on a hosted Windows runner and bind each capture to its executable hash. Check the normal 1200 x 800 and minimum 1000 x 600 client sizes, English, Cantonese and bilingual copy, light and dark themes, and 100%, 125%, 150%, and 200% display scales. For each tuple, inspect tab access, search controls, horizontal and vertical reachability, keyboard focus, and unintended clipping. Capture both the default Ink tab and the Objects tab corresponding to the supplied problem image. No new built capture or layout-probe receipt exists yet.
