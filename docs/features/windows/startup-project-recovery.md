# Recover a project that cannot reopen at startup

If the previously active project is missing, invalid, or interrupted while loading,
the application keeps its tab and file and opens a separate Untitled tab. The
recovery message explains two available actions:

- Select the retained project tab to retry it explicitly, for example after
  restoring a missing file or replacing a damaged copy.
- Use **File > Open** to choose another project.

Automatic startup restoration now uses an explicit context, separate from the
existing silent-import flag. It creates no loading progress dialog and uses a
nonmodal notification for failure or deferred review. Manual tab retry, File >
Open and other callers remain interactive by default, including their original
import diagnostics and safety decisions. Nothing automatically accepts a version,
G-code, printer synchronization, texture or scaling decision.

The initial automatic scope is deliberately conservative. The document must be
pristine, with no pending preset changes, selected device or pending wrapping
guidance. Detached parsing must identify a native project with a supported
version, no substitutions or embedded presets, no texture import, validated
system-derived presets and no modified G-code decision. Its normalized full
configuration must match the active configuration, including printer and bed.
Object and volume overrides other than filament assignment, and geometry needing
large-object scaling, require explicit review. Embedded-preset presence currently
defers restoration because the existing resolver has no read-only decision
preview, not because every embedded preset is unsafe.

Admission happens before plate, preset or settings publication. An admitted
automatic load keeps the already matching configuration rather than invoking the
preset editor, whose update path can show adjustment dialogs. A deferred or
pre-application failed load retains the pristine document without resetting its
presets. Only a failure after application began uses the existing recovery reset.
The retained target and pending marker remain available for explicit retry.
This narrower automatic scope is not a claim that every native project can be
restored without interaction, or that an un-restored project is corrupt.

The empty working tab has no file identity. Saving it therefore cannot overwrite
the retained project. A failed retry leaves the outgoing document active and keeps
the failed target available. Existing unsaved-document checks still apply.

Before automatic restoration, the application saves
`project_tab_recovery.pending_path` in its existing local configuration. A pending
marker is read back from bounded on-disk JSON before the automatic loader may run;
an unverified write leaves the target available for explicit recovery. A pending
path is not automatically attempted again after an interrupted launch. Successful
restoration, successful explicit tab retry, successful File > Open of the same
path, or adoption of that already-loaded startup project clears the matching
marker. Failed opens and successful opens of other paths retain it. Failed restoration
persists the separate empty active tab, so later normal launches do not repeatedly
attempt the retained invalid file. No model or snapshot is deleted by this recovery.
The marker is a local path, subject to the same privacy boundary as existing saved
project-tab paths; it is not exported or sent to a service by this feature.

Saved project files use the ordinary project loader. Private unsaved snapshots keep
their restore semantics and regain the original project filename after loading.
A document already established by another startup route is left in place and
bound to its matching or a new tab, including unnamed imported geometry and dirty
documents. If creating the empty recovery project is canceled because its current
history cannot be preserved, no empty tab is claimed or activated.

## Verification

The installed 2.9.1932 release, `md3-v260`, source
`dac16c242c95ac07c9913bf40cf05dbc508b69de`, executable SHA-256
`96d2c9b82ea2675a258221563b597bd9b84c190ffac73f9bedaf4725fd2af168`,
was compared using two owned, isolated data directories on separate hidden desktops.
Both configuration files ended with a newline. One contained a saved-tab reference
to an existing synthetic invalid `.3mf`; the control had no saved project tab.

The invalid case displayed **Loading of a model file failed.** The control reached
Home with an empty Untitled tab and no matching error dialog. Both actual images
were inspected. This demonstrates a reproducible saved-tab failure route; it does
not establish the cause of a particular user's startup report. Private run receipts
and captures remain outside the repository. No real user profile, project, or
personal vocabulary cache was manually edited. The blank control later showed a
network-plugin installation-completion dialog; no installation command was issued
by this verification. `--datadir` does not isolate the separate
personal vocabulary cache, so these runs performed no vocabulary import or reset.

`node tests/startup_recovery_behavior.test.mjs` compiles and executes the production
startup, explicit-switch, and rollback functions with deterministic loader,
configuration, and UI doubles, plus actual bounded marker-file parsing. It covers invalid/missing files, interrupted
restoration, successful restoration, explicit successful and failed retry, title
callbacks during loading, blank startup, existing startup documents, and private
snapshot identity, canceled reset, unnamed geometry, and failed configuration
writes. The changed source passes; the same executable check against
`d17122fa8ec2ae1da871e677aa3dce3beda94c92` reports 19 failed assertions. Matching
marker clearance is checked for explicit tab retry, File > Open, and already-loaded
startup documents; failed opens and other paths must retain the marker.

The same production-orchestration fixture also checks automatic versus manual
context, nonmodal recovery, decision-free restoration, deferred review without
simulated preset application, and explicit interactive retry. The
`--interactive-startup-mutant` option deliberately removes automatic admission and
must fail. `node tests/startup_nonmodal.test.mjs` executes the actual detached
admission block with neutral archive/configuration doubles and the production
progress wrapper. It checks an admitted case, eleven deferred classes, absence
of speculative application, released pending resources and interactive progress
cancellation. `--admission-mutant` removes the deferral branch and must fail.
The same fixture runs extracted downstream production conditions and placement
calls with geometry doubles: admitted automatic and manual native projects keep
negative Z and zero-volume objects, while manual geometry imports retain their
existing cleanup and placement path. `--classification-mutant` removes the
automatic project classification and must fail on those downstream effects.
These fixtures do not parse a real 3MF, exercise the actual preset editor or prove
that every indirect native path is nonmodal. Native build and scenario evidence
remain required.

The doubles do not prove native archive parsing, production AppConfig persistence, or
rendered recovery behavior. A native build and the same hidden-desktop scenarios,
including restart, repaired-file retry, File > Open, and unsaved outgoing content,
remain required before claiming end-to-end verification of the repair.
