# Recover a project that cannot reopen at startup

If the previously active project is missing, invalid, or interrupted while loading,
the application keeps its tab and file and opens a separate Untitled tab. The
recovery message explains two available actions:

- Select the retained project tab to retry it explicitly, for example after
  restoring a missing file or replacing a damaged copy.
- Use **File > Open** to choose another project.

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

The doubles do not prove native archive parsing, production AppConfig persistence, or
rendered recovery behavior. A native build and the same hidden-desktop scenarios,
including restart, repaired-file retry, File > Open, and unsaved outgoing content,
remain required before claiming end-to-end verification of the repair.
