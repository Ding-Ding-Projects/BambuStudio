# Regex builder

Every native search surface has direct access to the same full guided regex builder
(`RegexBuilderPopup`): wx surfaces use the shared MD3 `SearchField`, while the
font picker, generic ImGui list search, and assembly-tree search expose a
keyboard-reachable tune button connected through `RegexBuilderBridgeState`.
Both routes provide sectioned token construction, a raw pattern editor, flag
toggles, live syntax feedback, bounded sample-text testing with match and
capture-group listing, and copy/export against the app's real regex engine.

## Engine, dialect, flags, escaping

- **Engine**: Boost.Regex 1.84's wide-character engine with
  `boost::regex_constants::ECMAScript`, running only in the separate
  `bambu-regex-worker` process. The builder and all user-authored
  search paths use the same `BoundedRegex` client
  (`SearchField::textMatches`, `Search.cpp`, `ObjectDataViewModel.cpp`,
  `ImGuiWrapper.cpp`, and the assembly-tree filters), so preview and search do
  not drift between dialects.
- **Case sensitivity**: the *Case sensitive* flag maps to
  `boost::regex_constants::icase` (added when case sensitivity is off).
- **Multiline anchors**: the *Multiline anchors* flag clears
  `boost::regex_constants::no_mod_m`, so `^` and `$` also match line
  boundaries. It is off by default. `no_mod_s` remains set, so `.` does not
  start matching line breaks merely because multiline anchors are enabled.
- **Whole word**: applies to plain-text (non-regex) search only; in regex mode
  authors use `\b` themselves. The builder says so next to the flag.
- **Escaping**: a backslash escapes the ECMAScript metacharacters
  `\ ^ $ . | ? * + ( ) [ ] { }`. The *Literals* section performs this escaping
  automatically.

## Reaching the builder

- Each `SearchField` shows a persistent trailing `.*` **regex toggle** (plain
  text stays the default; regex mode is a deliberate opt-in that also switches
  the entry to Roboto Mono) and a `tune` **builder button** that opens the
  popover.
- Compact ImGui search rows show the same `.*` toggle plus a tune button. The
  bridge synchronizes pattern, regex mode, case sensitivity, and whole-word
  mode in both directions without capturing the lifetime of an ImGui owner.
- Search surfaces hosting the shared field include: parameter search
  (`Tab.cpp` / `Search.cpp`), the Plater object search (`Plater.cpp`),
  Preferences (`Preferences.cpp`), user presets (`UserPresetsDialog.cpp`),
  device selection (`SelectMachinePop.cpp`), the multi-machine manager
  (`MultiMachineManagerPage.cpp`), project version history
  (`ProjectHistoryDialog.cpp` — filters the snapshot list), the print host
  upload queue (`PrintHostDialogs.cpp` — find-in-queue: counts and selects
  matches; rows never hide because upload job ids are row indices), and the
  config profiles manager (`ConfigProfilesDialog.cpp`). Every one of them
  gets the identical builder. The ImGui font picker, generic list search, and
  assembly-tree filter use the bridge described above rather than a reduced
  toggle-only variant.
- **Colour-aware search:** rows that carry a colour also match by it. The
  shared helper `SearchField::colorSearchText(wxColour)` contributes
  `#RRGGBB <nearest-common-name>` (e.g. `#00AE42 green`) to the row's
  haystack; the Plater object search uses it so typing `green`, `grey`/`gray`
  or a hex value finds the objects printed with that filament. Regex mode
  composes naturally (`^#00` matches all dark-blue-channel hexes).
- Query, pattern, flags, and mode stay synchronized bidirectionally: typing in
  the field updates the popover's raw editor live, and edits in the popover
  re-fire the field's query callback so the host list re-filters immediately.
- The popover is **tabbed**: **Build** hosts the guided sections, and
  **Reference** is the built-in mini-documentation — how search works (plain
  text default, flags, fail-safe invalid patterns, bounded evaluation),
  every token with its full description (rendered from the same tables the
  chips use, so docs can never drift), worked examples, and the **OpenCode
  helper**: one button copies a prompt describing the engine, current
  pattern and sample text to the clipboard and launches OpenCode when it is
  on PATH (the prompt never goes onto a command line; nothing is sent
  anywhere by the app itself).

## Popover anatomy (`src/slic3r/GUI/Widgets/RegexBuilderPopup.{hpp,cpp}`)

1. **Title + engine caption** — names Boost.Regex 1.84's wide-character
   engine, the ECMAScript grammar,
   the isolated bounded worker, the `icase` flag, and the backslash escaping
   rule.
2. **Pattern** — raw pattern editor (Roboto Mono), bidirectionally synced with
   the owning field's query, plus a copy-to-clipboard icon button
   (`wxClipboard`).
3. **Validity line** — live feedback on every keystroke: Primary-coloured
   "Valid pattern" (with a match count when sample text is present) or an
   Error-coloured message mapping the worker's stable regex error code to a friendly
   description (unbalanced `[ ]` / `( )` / `{ }`, bad counts, invalid escape,
   invalid backreference, invalid range, dangling quantifier, unknown class,
   too-complex pattern).
4. **Flags** — *Regex mode* (the `.*` toggle), *Case sensitive*, *Multiline
   anchors*, and *Whole word* checkboxes. Flag changes re-fire the field's regex-toggle callback so
   consumers re-run their filter with the shared `textMatches()` semantics.
5. **Guided sections** — labelled chip palettes, each chip with an explanatory
   tooltip; clicking (or Enter/Space) inserts the token at the pattern caret,
   and `()` / `[]` style tokens land the caret inside the pair:
   - *Literals*: a text input plus **Add** that inserts the text with all
     metacharacters escaped;
   - *Character classes*: `.` `[ ]` `[^ ]` `a-z` `\d \D \w \W \s \S`;
   - *Anchors*: `^` `$` `\b` `\B`;
   - *Groups & alternation*: `( )` capturing, `(?: )` non-capturing, `|`,
     `\1` backreference;
   - *Quantifiers*: `*` `+` `?` `{n}` `{n,}` `{n,m}` and the lazy variants
     `*?` `+?` `??`.
6. **Test pattern** (collapsible, collapsed by default — progressive
   disclosure keeps the popover compact) — a bounded multiline sample-text
   input with live match highlighting (SecondaryContainer tonal spans) and a
   results list showing each match's ordinal, character range, matched text,
   and every capture group (`group N: text` / `group N: (no match)`).

The popover hosts real child controls inside a transient popup using the
`wxPU_CONTAINS_CONTROLS` pattern proven by `Search.cpp`'s `SearchDialog`.
Showing the popup and focusing its raw-pattern child are one operation
(`Popup(m_pattern)`): wxWidgets installs focus tracking around the actual
child before activation changes, and its native transient-window handler owns
outside-click/Alt+Tab dismissal. A second owner-deactivation hook must not be
bound because it would dismiss the popup while focus is entering the pattern
editor. `Esc` dismisses it explicitly. Content lives in a vertical
`wxScrolledWindow` whose
height is capped to the display (fits an 800px-tall screen at 100–200% scale;
overflow scrolls). The popover is rebuilt on every open so colours, fonts and
`FromDIP` metrics re-derive per open — theme, DPI, and density safe. All
colours resolve through `StateColor::semantic(MD3::Role::...)`; there are no
hardcoded light-mode literals. Every action has at least a 44-DIP target. Each
guided token is a real, independently focusable pushbutton with its own
accessible name and tooltip; normal Tab/Shift+Tab navigation and Enter/Space
activation come from the same shared button control used elsewhere in Studio.

## Matching semantics (`SearchField::textMatches`)

- empty query → matches everything;
- plain text → substring test honouring *Case sensitive*; *Whole word*
  constrains hits to `\b`-style boundaries without regex parsing;
- regex → deadline-enforced Boost.Regex wide-character search in
  `bambu-regex-worker`, with
  `icase` when case-insensitive; an invalid, oversized, timed-out, or
  unavailable evaluation **matches all** (`true`) so a list is never blanked
  mid-edit.

## Configuration

Nothing is persisted beyond the per-field mode toggle; patterns, flags, and
sample text are session state. `SearchField::GetPattern()` exposes the
current pattern to hosts. Native raw search/pattern inputs enforce the same
512-code-unit protocol cap before dispatch. Literal insertion is atomic: the
fully escaped value is inserted only when it fits the remaining pattern budget,
so metacharacter expansion cannot allocate or insert beyond that cap.

## DeviceWeb Filament Manager search

The embedded Filament Manager has its own web-native implementation because
its real search engine is JavaScript rather than native Boost.Regex:

- The toolbar search remains case-insensitive plain text by default. Its
  adjacent, 44-DIP `.*` button opens the complete MD3 builder; toolbar
  query, raw pattern, `i m s u` flags, and mode synchronize in both directions.
  Local spool rows already held by the native store, filters, search, the
  builder, grouping, and activity history remain available while signed out or
  offline. Add/edit/delete are cloud-backed rather than durably queued offline,
  so they and explicit cloud pull/push controls require a cloud session. Mount always
  refreshes native local presets, but it neither force-fetches cloud config nor
  triggers a cloud pull until the bridge confirms an authenticated session.
  Authenticated cloud-config requests carry a lifetime-and-generation ticket:
  validity is checked at network completion and again after the GUI-thread hop,
  VM destruction invalidates queued callbacks, and a newer request supersedes
  any older response so stale data cannot overwrite the cache.
- The builder identifies the dialect as JavaScript `RegExp` (ECMAScript),
  explains slash/backslash escaping, and provides escaped literals, character
  classes, anchors, capture/non-capture groups, alternation, and quantifiers.
  A bounded sample editor shows live ranges, numbered and named captures, and
  explicit truncation notices. Copy returns only the raw pattern; JSON export
  contains dialect, mode, pattern, and flags but deliberately excludes sample
  text. Copy/export results use the corner notification stack: successes time
  out, failures remain until dismissed, and both stay reviewable in Activity
  History. Guided tokens and escaped literals are atomic at the 256-code-unit
  limit: the complete insertion is applied or the pattern remains unchanged
  and a persistent warning explains why.
- Compilation and matching occur only in a disposable Web Worker. Each edit
  starts a fresh worker; the page terminates it on completion, cancellation,
  error, or the non-widenable 175 ms wall-clock deadline. The worker also
  checks a 100 ms cooperative CPU budget between engine calls. There is no
  in-page regex fallback.
- Bounds are enforced before structured cloning and again inside the worker:
  256 pattern code units, 8,192 sample code units, 5,000 candidates, 2,048 code
  units per candidate, 1,048,576 aggregate candidate/id code units, 100 preview
  matches, 20 numbered/named capture groups, and 512 code units per displayed
  match or capture value. Zero-width matches advance by a Unicode code point
  under `u` and otherwise by one UTF-16 code unit.
- Pending, invalid, oversized, timed-out, unavailable, and crashed evaluations
  fail open to the already tab/filter-restricted base list. This prevents a
  half-typed pattern or worker fault from blanking the table or clearing its
  current selection.
- The builder is a modeless, portalled right-side tool surface, not a decision
  dialog: the underlying page remains operable, focus is not trapped, Escape
  closes it, and close restores the invoking control. Its one-column narrow
  layout avoids horizontal clipping. Tabs use tablist/tabpanel semantics and
  arrow navigation; filter triggers expose expanded and controlled state, and
  their radio menus support arrows, Home/End, Escape, focus restoration, and
  44-DIP targets. Toolbar grouping exposes pressed state; group, cloud, push,
  history, add, and batch actions also meet the 44-DIP target. English, Hong
  Kong Cantonese, and the derived bilingual mode use the same translation-key
  parity check.

Verification lives in `device_page/tests/filamentRegex.test.ts` and covers
plain/regex state synchronization, invalid and hostile inputs, the hard worker
kill path, Unicode, multiline anchors, zero-width advancement, captures and
capture bounds, fail-open behavior, deterministic Turkish-I casing, atomic
exact-limit insertions, offline local-library access, delayed callbacks after
owner destruction, out-of-order cloud-config responses, mutation caps,
pre-clone rejection, push-batch cleanup, and the two-ID candidate-prefetch
race. Both CMake's `device_page_build` and the Windows preflight run that suite
plus DeviceWeb language parity before building the bundle. Their frozen pnpm
installs run with `CI=true`, and the build contract rejects disabled TLS or a
missing mandatory hash on either bootstrapped executable download.

## Security considerations and bounds

- Patterns and sample text are evaluated **locally only** — never
  transmitted, logged, or persisted.
- **Pattern cap**: 512 `wchar_t` code units (enforced by the editor, app
  client, wire decoder, and worker before compilation).
- **Subject/sample cap**: 8192 `wchar_t` code units. The builder sample editor
  truncates explicitly and reports that fact; an oversized programmatic search
  candidate is rejected and fails open (it is not silently truncated and
  therefore cannot produce a misleading partial-string result).
- **Match cap**: the first 200 matches are listed/highlighted. The truncation
  notice appears only when a 201st match actually exists, not merely when the
  result count equals the cap.
- **Structural cap**: nesting is limited to 32 levels; quantifier and
  alternation counts and counted repetitions are bounded before the request
  leaves the app and rechecked by the worker.
- **Wall-clock cap**: each compile or match request gets 50 ms by default
  (callers may request 1–250 ms). The app uses framed local IPC to a persistent
  helper. On deadline expiry it terminates that process, returns a fail-open
  result, and starts a fresh helper for the next safe request. There are no
  detached regex threads: application startup prewarms the contained helper on
  an owned background future, and a filtering call that overlaps startup or
  another RPC returns fail-open immediately instead of queuing the UI thread.
  Process launch, containment setup, and the no-regex readiness ping are
  separate from the evaluation budget; the ping has a two-second deadline and
  a helper that never answers is killed. Only after readiness succeeds does the
  caller's unchanged 1–250 ms compile/match deadline begin. Startup failures
  report their exact stage and retry with a bounded backoff.
- **Aggregate filter-pass cap**: collection filters create one `SearchPass` for
  their fixed pattern and flags. Its candidates share one deadline; the first
  timeout, worker/protocol fault, resource rejection, or aggregate-budget
  exhaustion opens a circuit and all remaining candidates in that pass fail
  open without another IPC wait. The aggregate deadline begins after successful
  pattern validation/readiness, and a new filter pass gets a fresh circuit.
- **Memory/isolation cap**: Windows places the helper in a kill-on-close Job
  Object with a 128 MiB process-memory limit. Linux uses a 128 MiB address-space
  limit and an 8 MiB stack limit. On Darwin, current XNU enforces `RLIMIT_AS`
  through its Mach VM map-size limit; the worker records its post-exec mapping
  baseline and installs hard `RLIMIT_AS` and `RLIMIT_DATA` limits that allow at
  most 128 MiB of additional mappings, plus an 8 MiB stack limit. The macOS
  parent also checks `proc_pid_rusage` physical footprint at most every 2 ms
  while a request is active and kills the helper above 128 MiB. Windows uses an
  explicit inherited-handle list; the POSIX post-fork path closes every
  descriptor above stderr before `exec` and explicitly clears close-on-exec on
  stdin/stdout. Thus only the intended protocol channel is inherited. The
  helper has no network or persistence path.
- **Engine defense in depth**: the worker caps Boost.Regex at 1,000,000 visited
  states. Boost.Regex can raise `error_complexity`, `error_space`, or
  `error_stack`; these map to a stable too-complex result.
  The process deadline and memory limits remain the security boundary rather
  than relying on implementation-specific exception timing.
- **Timeout caching is request-specific** (pattern, flags, operation, subject,
  and match cap). A hostile sample cannot poison the same pattern for a later
  harmless subject. Syntax and compile-structural failures may be cached by
  pattern because they are subject-independent.
- Zero-width matches advance with `boost::wsregex_iterator` semantics, stop at
  the 200-match cap, and are never painted as empty highlights.
- The production helper is resolved only beside the running executable. The
  Windows installer places `bambu-regex-worker.exe` beside `bambu-studio.exe`;
  supported non-Darwin POSIX packaging installs `bambu-regex-worker` beside
  `bambu-studio`. macOS bundles the architecture-matched helper in
  `BambuStudio.app/Contents/MacOS`; universal builds merge both helper slices
  just like the main executable before final bundle signing.

## Failure modes

- **Invalid pattern** → red friendly message in the builder's validity line;
  the field itself degrades to match-all so the host list stays populated.
- **Pathological pattern** → the worker reports its engine complexity guard or
  is killed at the wall-clock deadline; the builder reports "Pattern too
  complex to evaluate safely" and search degrades to no-filter.
- **Oversized inputs** → the builder sample is explicitly truncated with a
  notice; an oversized programmatic candidate fails open without partial
  evaluation.
- **Missing/crashed helper or malformed IPC** → no in-process fallback; search
  fails open and the builder reports that regex evaluation is temporarily
  unavailable. A later request starts a clean helper.
- **Clipboard busy** → copy is skipped silently (no data loss; pattern stays
  in the editor).

## Verification

- Register row `no-shared-md3-searchfield` (design-system parity register):
  done.
- Regression coverage must open the builder through the tune button on every
  `SearchField` adopter, confirm the raw editor owns focus, then exercise
  outside-click and `Esc` dismissal. Testing the adjacent `.*` toggle alone
  does not prove that the transient popup survived activation.
- Search fields and the builder popover captured per button in the screenshot
  matrix under `docs/screenshots/regex-builder/`.
- `bounded_regex_tests` is a real CTest/CI target. It covers valid, invalid and
  no-match results; Unicode and capture groups; explicit multiline mode;
  zero-width advancement and match caps; app and protocol bounds;
  plain-text-versus-regex behavior; missing-worker fail-open handling; and an
  adversarial nested-quantifier timeout followed by worker restart/recovery;
  an aggregate deadline across 100 distinct hostile candidates; exact match-cap
  semantics; nonblocking delayed prewarm followed by the first successful
  `SearchPass`, and a started helper that never answers its ping; builder/ImGui
  bidirectional state; independent lower/upper counted-repetition bounds; and
  POSIX descriptor isolation, closed-parent-stdio launch, and finite worker
  resource limits. On macOS, a platform-specific test proves that the contained
  worker remains functional for valid, invalid, match, and no-match requests.
  CI also configures the test
  tree with `SLIC3R_GUI=OFF` and `SLIC3R_BUILD_TESTS=ON`, keeping this focused
  engine-safety suite independent of the GUI target.
  The Windows workflow builds this target and runs it through CTest on every
  push and manual dispatch. Each macOS runner also configures the headless
  target for its native architecture and runs the same CTest gate after the app
  build, so the Darwin resource-limit and functional path is compiled and
  exercised on both Intel and Apple Silicon jobs.
- Visual/manual coverage remains useful for focus, copy, localization, and
  highlight presentation, but it is not evidence for the evaluator's safety
  boundary; the automated worker tests are.
