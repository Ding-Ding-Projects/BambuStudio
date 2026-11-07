# Current implementation handoff

Mixed-version compatibility: protocol-1 status includes directSend:true and an inert dropCode value of not-required for older receivers. The authenticated legacy renewal endpoint returns 410 retired, never a successful rotation or revocation. An older desktop may briefly show a connection error after trying renewal; the next status poll restores collection. The inert status value is never an upload credential. New receivers keep collecting from older services but hide direct invite links until the service advertises the capability, and show an upgrade instruction. Older desktop versions retain their old code labels until updated.

## Direct LAN sending, 7 November 2026

Sender drop codes and renewal controls are removed. Open the invite link or scan its QR code, choose models and send directly. Station keys still protect inbox, downloads and management; local pipe pairing and DPAPI storage are unchanged. Model, size, queue and expiry checks remain, with 60 upload attempts per source address per minute and a five-minute cooldown. Existing volumes, keys and waiting models are retained.

Source implementation passed 95 focused API, page-contract and compiled native-helper checks, including mixed-version capability handling and Unicode filename transport. Container version 1.0.1 publication, live sender interaction and native UI acceptance remain pending; historical 1.0.0 evidence does not verify this change. Use only a trusted LAN and never expose the service to the internet.

Implementation checkpoint: d5a4eb56fcbb2c10a701aaad155b0d7af4fe32f2. This continuation integrates parent candidate 4f7039f611477b4f7ba4623309b6dfd594ce8af9, removes external code-row callsites, translates the two new invite messages and refreshes the embedded documentation. No release or runtime success is claimed. The isolated branch is gap/preferences-responsive-20261107; remote preservation remains the integration owner's next step.

Objective: complete the approved required-feature plan, including native and
companion surfaces, with priority repairs for Preferences sizing, personal wording,
LAN model-drop startup and the recurring startup model-load error.
Latest direction: remove the sender drop-code requirement throughout the upload
page, service, invite links and native controls. Keep station management authenticated.

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

Hosted native compilation and Squirrel packaging of 6eb975873 passed in
run 42414873193. Its earlier image-publication job failed authentication; the
separate repaired run 42415338522 succeeded at source 8287f75cb. The published
image was pulled locally and Compose is pinned to its verified digest. Real
host.bat detached and log modes passed startup, pause/follow/quit and continued
health after exit. Health also passed from a second LAN host.

Local bootstrap now correctly rejects MSVC 14.44. After fresh installation and
modification could not restore an incomplete registration with missing payload,
the targeted vendor repair succeeded. Root bootstrap verifies the project-owned
VS2026 registration is complete and the toolchain is ready. Unrelated IDEs remain
unchanged. Reviewed sound controls at fa3ea7861 are integrated; native sound proof
is pending. Two new Cantonese strings and the documentation bundle were updated.

The packaged 6eb975873 candidate reproduced the synthetic first-load error and
reached its recovery dialog. The failed tab and original file were preserved,
with a separate active Untitled tab. Restart proof is pending. Preferences resized
from 780x640 to 1050x720 through the identity-bound hidden-window route, but its
images were black and rejected as layout evidence. A bounded diagnosis is active.

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
