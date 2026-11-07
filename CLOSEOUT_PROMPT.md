# Current implementation handoff

Objective: complete the approved required-feature plan, including native and
companion surfaces, with priority repairs for Preferences sizing, personal wording,
LAN model-drop startup and the recurring startup model-load error.

The user confirmed that the model-load message appears on startup before opening
any project. An owned synthetic invalid saved project reproduces the installed
message; this is not proof of the user's actual affected file.

Working branch: gap/feature-integration-20261107. Startup source is preserved on
origin/gap/wording-display-20261107 at 55fe0ccaa6afe628253a4825d8a1990f3d927eb2.
Preferences and catalogue source is preserved on origin/gap/preferences-responsive-20261107
at c4a81c2471365fefcad0a50729228924bfebcbfe. Both were verified with remote-ref readback.
The integration branch contains both contributions plus hosting and build repairs;
its latest candidate still needs remote readback after this record is committed.

Completed local checks: startup production-transition fixture passes, earlier
source fails 19 assertions; 12 entrypoint assertions; successful prerequisite
bootstrap; six container and actual CMake-install checks; 28 API checks; 10,246
complete Cantonese catalogue entries. These are not native runtime or release proof.

Hosted native compilation of 6eb975873 passed; packaging is still in progress in
run 42414873193. Its earlier image-publication job failed authentication; the
separate repaired run 42415338522 succeeded at source 8287f75cb. The published
image was pulled locally and Compose is pinned to its verified digest. Real
host.bat detached and log modes passed startup, pause/follow/quit and continued
health after exit. Health also passed from a second LAN host.

Local bootstrap now correctly rejects MSVC 14.44. The signed VS2026 installer
returns 1 because its project-owned installation path has an incomplete existing
registration. A supported scoped modification is being prepared; unrelated IDEs
must remain unchanged. Action-sound controls are being implemented separately.

Next: obtain the exact packaged candidate, verify startup/retry and resizable
Preferences on the named hidden desktop, verify personal wording without exposing
private data, verify installed native hosting/pairing, and complete companion and
full feature acceptance. Finish the scoped compiler bootstrap recovery separately.
The accepted installed application has not yet been updated. Main integration,
release publication, deployed documentation and final cleanup remain unfinished.

Publication scanning remains incomplete because of existing technical-identifier
findings, nested-repository coverage and a full-workflow Markdown-parser failure.
Preserve that limitation; do not claim a clean full scan from supplementary checks.
Some task-owned hidden baseline processes remain after normal close attempts;
retain their private identity receipts and complete identity-bound teardown.
Status Hub delivery through the authenticated host-local SSH client remains pending.

Keep unrelated work and user projects intact. No force pushes, public private-wording
payloads, host power/login actions or unverified completion claims. The active goal
remains unfinished; account allowance is unknown. No cleanup authority extends beyond
proven task-owned, preserved, integrated candidates.
