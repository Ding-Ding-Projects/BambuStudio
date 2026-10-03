# Native interface continuation

## Objective and constraints

Complete MCP automation; responsive native transitions and asynchronous work;
Material Design 3 details; search and regex builders in every context menu;
clipped or hidden text repairs; unintended slice cancellation; adjacent Slice
and Print and Slice and Send actions; and local personal-vocabulary JSON
import, validation, persistence, replacement, clearing and original wording.
All new implementation and independent review lanes use gpt-6-astra.
All product builds, tests, packaging, installation, slicing and GUI execution
run only on GitHub-hosted Windows runners. No local product execution occurred.

## Preserved candidates

- MCP: ce61d22e390e4bf69938730434f1e494fa34f7a9 on feature/mcp-integration.
  Run 37054493889 passed 27 managed checks with zero failures or skips;
  native compilation and release publication passed. Release md3-v187 binds
  that source; installed runtime run 37084928659 is pending.
- Shared menus and profile recovery: 45f1101542c239d14713a3ff705da281a9dbc0d4,
  followed by profile reservation and corrupt-archive checks in
  13ce9d652220a489c965561aea313d92a859430c. Independent source review accepted
  the latter fixes; hosted execution remains pending.
- Personal wording: 9ec8e9604b9fbbd2cb3108b1f03f209042c96a33, with native-label
  getter and concurrent-cache fixes at 26526edc1f43996d7b467962b63ee0471742ab1f.
  Independent source review accepted both repairs; hosted verification is pending. Only synthetic
  neutral mappings may enter public tests, logs, captures or hosted payloads.
- Canvas: 4eba2c62a0265d950c172e63ed711db83e3b0f04 on feature/ui-canvas-menus,
  with 46 model assertions passed on hosted run 37055936191. Independent source review
  accepted the bounded transient retry repair.
- Slicing: a3143f67d60dcb93f2e766dcd51c2a41182a9e03 is pushed and verified on
  feature/slice-print-send. Independent source review found no additional
  accepted defect. Five C++ cases (28 assertions) and eight source mutation
  contracts passed in hosted run 37055936191. Synchronous model replacement and shutdown can still wait
  for a noninterruptible geometry operation; user cancellation is nonblocking.

The parent reused its idle integration checkout on feature/ui-integration,
combining the shared controls, profile repairs, canvas and slicing candidates.
It also incorporates the display boundary repairs and generated test catalogs
from 29fdc865c5e554cb76ad9ca7a544d6911f7e5e21.
This is not main and does not establish runtime correctness.

## Work in progress

Runtime verification is implemented through 7c0913c2cdd87792d9d089bbd7719f6698d8fa87:
original native label checks plus rendered vocabulary changes, restart and invalid
input; synchronized optional worker observations; bounded actual cancel targeting;
disabled overlap prevention and separately reported stale completion evidence.
Independent review accepted the final freshness repair. Seven focused helper tests
await hosted execution. Fixture-path repair c6a7d10e701d631679d2a355b2575eae18558d4d
normalizes invocation paths before selecting the source root, with three tests
covering both drivers. No actual printer submission is authorized.

Run 37085064156 passed at 6e3cda2facd208a4d6521d0539ec0346aa8a8e50, including
all focused native services and sequential/concurrent vocabulary persistence.
Installed MCP run 37084928659 validated and installed md3-v187 but failed on the
fixture lookup before exercising MCP. Its repair needs a new candidate runtime run.
The installed-interface workflow is now wired for each bounded scope and tuple.

The personal-wording owner is frozen after independent review.
A separate feature/preview-layout candidate 35ce8dbcbf5d695425e64f652054790c332ee891
repairs notification and preview geometry and passed independent source review.
It is pushed and merged into the integration candidate. Its 68 checks passed
in hosted run 37056354867, alongside 46 canvas checks and 55 source tests.
A feature/ui-runtime-verification lane implements genuine
hosted menu, vocabulary and slice-control interactions using the existing
restricted evidence route. The parent owns focused hosted workflow wiring,
root documentation, integration and publication evidence.

The earlier MCP-only run 37045639569 completed successfully at
2026-10-02T19:38:58Z and published md3-v185 for
65dc4577fb29f2d00fd525a1de11e370784c5b3e. Its five installer/feed/checksum/SBOM
assets exist. It does not contain the newer interface or vocabulary work.
Its obsolete runtime driver is not used to verify the current candidate.

## Next steps and boundaries

Run 37055936191 passed the focused job but failed native-service compilation:
the Cantonese catalog requires reviewed-category metadata on new entries, and
PersonalVocabulary.cpp:231 selected std::apply through argument-dependent lookup.
Both are repaired and independently reviewed at
c68cf438413b99633eb376ad89c2d4688dc95d20. The next run 37056354867 passed
focused checks but failed linking wxWidgets assertion symbols. Repair
fd0067052fc75c0f7395b5f343f06cfd818bf3a9 matches the production static ABI.
Full build 37056348902 also failed on an ambiguous preview begin overload;
59d5c0b34b873a12048774799cf414bf9ab2700c selects the intended string overload.
Both repairs are integrated on the candidate and await hosted verification.
The focused job also passed 49 source tests with zero failures.
The focused native-service job requires the dependency prefix produced by the
existing native build and fails explicitly on a cache miss. It builds generated
translation catalogs before running language checks. It does not replace a full
application build or installed UI verification.
Complete preview repairs and genuine installed interaction evidence across
normal/minimum dimensions, English/Cantonese/bilingual, light/dark and measured
100/125/150/200% display scales. Do not call source changes visible success.
Preserve final device selection and confirmation; no real printer action is
part of verification without separately authorized hardware.

Integrate completed verified work into main, push and verify its remote SHA,
and obtain the final hosted verdict. Retain all worktrees until their tips
are proven ancestors of remote main and a complete verified external backup
exists. No cleanup has run. Status Hub enrollment remains unavailable, and no
status delivery is claimed. A pending run is never a successful verdict.
