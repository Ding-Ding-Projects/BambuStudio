# Native surface registry

`FeatureServices::SurfaceRegistry::instance()` maintains weak references to live
wxWidgets surfaces. Call `register_surface(window, "semantic-id")` for each
independently addressable surface. IDs accept ASCII letters, digits, `_`, `-`,
`.` and `/`, are bounded to 256 bytes, and must not contain user content.
Duplicate live IDs are rejected. Explicit IDs are stable across reconstruction.

Children are discovered at registration, on idle, and synchronously before a
producer permission query. Generated IDs combine the parent ID, native class
name and first free ordinal. Existing live children retain their IDs. A destroyed
child's slot is reused by a replacement of the same class. For collections that
change order, assign explicit semantic IDs: structural slots do not identify a
document or item across arbitrary reorderings. An automatically discovered child
can later be promoted to an explicit surface.

Every discovered window is adopted by the existing `ElementStyle` implementation.
The registry does not install a second context menu. Its stable appearance ID is
available through `surface_id()`, and existing appearance overrides and change
notifications apply to that ID.

## Presentation

Register original public message strings with `record_label()` and
`record_tooltip()`. A presentation refresh translates those stored originals
through `I18N`, then applies the existing display-only personal wording adapter.
It never translates the current rendered label back into another language.
Vocabulary refresh observers reapply these originals after load and clear.
`refresh_presentation()` also invalidates layout and reapplies appearance.

For fixed factual disclosures, `record_label_renderer(window, original,
renderer)` retains the explicit original and uses the provided stateless renderer
on every refresh. Its result is final: the registry adds no tone or personal
wording transform. The owner supplies the real factual translation function,
without reading field values, inferring originals or synthesizing account state.
An empty renderer is rejected without replacing the existing source. A later
`record_label()` call restores the normal presentation path. Both registration
forms reject sensitive surfaces and text entry controls. Dynamic code/date/status
labels remain unrecorded.

Original source registration is explicit. Text entry values, document content,
filenames, custom-drawn controls, menus and list item contents are not inferred
or rewritten. Their owning producer must provide its own source-aware adapter.
The registry changes no saved document, exported text or translation catalog.

`record_name(window, public_source)` sets a translated native accessible name
using `SetName`, including on masked and explicitly sensitive controls. Supply
only neutral public purpose text such as "Password confirmation", never entered
content. It never reads the control's current value or label, never changes those
values, and never relaxes sensitivity. Marking a field sensitive clears recorded
label and tooltip sources but preserves its explicitly registered neutral name.

An absent `record_label()` call leaves that label entirely component-owned.
`clear_label_source()` removes an earlier recorded source without changing the
current label, allowing a component to resume updating dates, codes or status
text without a later presentation refresh overwriting them. Name and tooltip
sources remain independent from this label opt-out.

## Sensitive surfaces and producer checks

Call `register_sensitive()` before displaying a sensitive surface. The flag is
sticky for that native window's lifetime. Masked `wxTextCtrl` fields are also
recognized directly from their password style, including before idle discovery.
History, export and capture producers must call their matching permission query
immediately before collecting content. The query rejects unknown windows,
unregistered enclosing subtrees, any sensitive ancestor, and any sensitive
descendant. Thus a screenshot of a parent cannot include an excluded child.
Destroyed windows are removed through weak-reference pruning.

On Windows the registry attempts `SetWindowDisplayAffinity` with
`WDA_EXCLUDEFROMCAPTURE` on the entire top-level window containing sensitive
content and reads back the result. `capture_protection()` distinguishes applied,
unsupported, failed and not-required states. Other platforms report unsupported.
The optional observer reports changes without surface labels or content. The
previous affinity is restored when sensitive content disappears. Affinity is an
additional OS capture mechanism, not a guarantee against every capture route or
a replacement for checks in application capture producers. Metadata alone does
not protect an image, export or history entry.

## History identities

`history_identity()` returns a distinct 32-character lowercase hexadecimal ID
only for a history-permitted surface. The mapping is stored under the fixed
`Slic3r::data_dir()` location `surface-identities/identities.json`, keyed by
public structural IDs rather than labels or input values. New identities use
random bytes, are collision-checked and are published only after atomic file
replacement. A cross-process lock prevents lost updates. Storage is bounded to
4096 identities and 1 MiB. Reads reject duplicate keys, unknown fields, excessive
nesting, invalid values and repeated identity values. Invalid storage, lock contention and write failures
return an empty identity, so the producer must decline recording. Sensitive
surfaces never obtain an identity through this API.

## Verification and limits

`tests/feature_services/surface_registry_tests.cpp` is a native integration test
for unknown windows, dynamic children, replacement IDs, explicit promotion,
duplicate IDs, actual appearance adoption, original-source refresh, sensitivity
inheritance, whole-parent exclusion, destruction, and masked inputs. It requires
the production wxWidgets, appearance, translation and wording implementations
and a native display. Its source is not evidence that it ran. Native build and
runtime execution must be recorded by the application integration owner.

This registry is UI-thread-only. Producer wiring is required at every collection
site. It does not protect files already collected, automatically discover secret
text in ordinary controls, or claim coverage for surfaces never registered.
