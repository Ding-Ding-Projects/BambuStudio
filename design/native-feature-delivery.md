# Native feature and surface delivery ledger

This is the explicit starting inventory for the native Windows application and its
product-owned embedded web views. The standalone documentation website is outside
this lane. The machine-readable record is [native-feature-delivery.json](native-feature-delivery.json).
It is an acceptance backlog, not a claim that the application has every feature.

The bounded source audit used `9a55b7aa1e900f85c2ded1389854beefabff159f`. This lane
starts from fetched main `3fa5dd4a48ba21e5b286f29035fb9b0e892abbe3`. Source modules
are candidate reuse points. Their presence does not prove any particular surface,
runtime behavior, hardware result or screenshot. No current product interaction,
build, installer execution or capture was performed by this inventory lane.

## Reading the record

There are 28 explicitly named surface groups and 43 independently enumerated
feature families, with one record for each of the 1,204 required pairs. Each pair
starts with unknown source coverage, unverified documentation/localization/
persistence/tests/runtime, and missing current captures. Family-level source
findings retain the narrower present/partial/missing/unknown conclusions and exact
candidate paths. They are not inherited as surface-level acceptance.

These groups are the minimum first inspection boundary. Before acceptance, split
each group into every reachable screen, nested panel, dialog and declared state;
keep stable identifiers and add each obligation to the independently maintained
test list. The generic state set covers normal, hover, focus, pressed, selected,
disabled, dragged, validation, loading, success, warning and error. A grouped row
or a source-only status does not discharge those individual state obligations.

For each real surface, populate exact implementation, article, localization,
persistence and test references plus built interaction and capture evidence.
References must identify the actual source revision and state, not just a filename.
Any deliberate deviation needs its exact reason and approval reference. Unknown
conditional applicability remains unresolved; it is not a silent exemption.

## Preserve and reuse

- Keep existing printer/model/save/import semantics and real action callbacks.
- Use the existing native design tokens, registered widgets, tab strip, menus,
  bounded regex worker, palette, histories, export serializers and offline docs.
- Model the new workflow as Prepare, Preview, Print and Monitor while retaining
  the existing final print confirmation and current plate/generation checks.
- Reuse the existing motion policy; the operating-system reduced-motion request
  always wins. Add transitions only where they preserve readability and state.
- Retain [native-print-workflows.md](native-print-workflows.md), the vendored
  `ui-md3/design-system/` references and the existing layout defect inventory.

## Prioritized bounded ownership

1. Surface registration and deterministic routing, exact feature/state identities,
   per-surface search/palette hooks and evidence schema.
2. Native workflow shell and shared layout primitives, then embedded-web adapters.
3. Shared private state: School mode, credential vault, element locks, authenticator,
   support recovery and protected append-only history.
4. Complete appearance/property/state renderer, layered editing, typography,
   continuous colors, logo customization and private bounded conversion.
5. Discovery/collections: searches, regex capabilities, groups, bulk actions,
   faithful exports, external-editor handoff, defaults and provenance.
6. Separate local-tools families: converter/adapters, Ollama manager, narration
   voice/completion routing, schedules, attention modes and product-owned recovery.
7. Real built inspection and exact-tuple repair, then reference comparisons and
   complete acceptance. Source tests never replace this final evidence.

No lane may assume that a narrow existing feature satisfies a broader family:
printer vision is not the full Ollama suite; qpdf staging is not a converter;
opt-in scalar appearance styles are not a complete layered editor; quiet defaults
are not the five attention modes; a default SAPI voice is not multilingual voice
selection and completion sequencing.

## Verification contract

Run only the focused inventory test for this lane:

```text
node --test ui-md3/tests/native-feature-delivery.test.mjs
```

The test independently lists required surfaces and families. Removing a row,
surface or family cannot remove the requirement by discovery. It also rejects
duplicates, incomplete tuple coverage, unsupported statuses, nonexistent candidate
source paths, proof-free verified claims and a false product completion claim.
`NATIVE_LEDGER_REMOVE_ROW=1` is an explicit test-only negative fixture: it removes
one in-memory obligation and must make the normal completeness test fail. Unset
it to restore the identical input and obtain the green result. Additional negative
cases cover an embedded-web row, a whole family, false proof and duplicates.

This test verifies the inventory boundary and honest status shape. It does not
verify implementations, documentation contents, pixels, interactions, privacy or
the correctness/freshness of subsequently supplied evidence. Evidence acceptance
requires separate type-specific receipts and actual reviewed runtime results.

## Built evidence required

Normal client size is 1200 by 800 DIP. The live minimum is
`GUI_App::get_min_size()`: `max(1000,76*m_em_unit)` by `max(600,49*m_em_unit)`.
Measure the real client rectangle rather than assuming the outer frame has that
size. Cover English, Cantonese and bilingual; light/dark; comfortable/compact;
100/125/150/200 percent scales; and applicable reduced-motion states.

Each interaction/capture carries exact source commit, build receipt, binary hash,
screen/state, viewport, language, theme, scale, input route, accessible target,
privacy verdict and image hash. Native geometry comes from the application's
layout probe; embedded web regions additionally require actual computed layout.
Reference comparisons use identical deterministic tuples and preserve both raw
captures, a labelled comparison, machine-readable diff and reviewed deviations.
Missing hardware/account/capture capability stays explicitly unverified.

Existing tools: `scripts/md3/recapture.py`, `scripts/md3/capture-tuple.py`,
`scripts/md3/send-layout-probe.py`, and
`ui-md3/tests/layout-probe-report.mjs`. Discover live targets before using older
coordinate recipes. Never present blank web/canvas captures, design previews or
old manifest rows as current product acceptance.
