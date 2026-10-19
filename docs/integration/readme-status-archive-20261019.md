# README status archive, 19 October 2026

This file keeps, verbatim, the dated status paragraphs that opened `README.md` before its 19 October 2026 rewrite into a tabbed README. They are historical records of their own dates and are not the current state; the current state is in [`HANDOFF.md`](../../HANDOFF.md), [`ROADMAP.md`](../../ROADMAP.md) and [`CLOSEOUT_PROMPT.md`](../../CLOSEOUT_PROMPT.md). Nothing below was edited.

## Opening status block (before the title)

Installer repair, 9 October 2026: a registered Visual Studio compiler inside the Squirrel install destination prevented `md3-v234` Setup from replacing that directory. The compiler bootstrap and PDF build default now use `%LOCALAPPDATA%\BambuBuildTools\VS2026`, outside the installer destination. The affected local instance was migrated through Microsoft's installer, and the unchanged released Setup completed successfully. The installed application remained running with an isolated profile. [Repair evidence](docs/evidence/installer-toolchain-separation/receipt.json) and [bootstrap behavior](docs/features/releases/windows-one-click-build.md#compiler-and-installer-directory-separation) distinguish this focused verification from broader product acceptance.

![Installed md3-v234 application after the compiler directory migration](docs/evidence/installer-toolchain-separation/installed-v234.png)

All reviewed local source continuations are integrated, and only the primary working directory remains registered. The [closeout report](docs/integration/closeout-20261006.md) records passing hosted workflows, verified backup consolidation, and two retained directory remnants. [Issue 60](https://github.com/Ding-Ding-Projects/BambuStudio/issues/60) tracks unfinished native and website acceptance; source integration is not product acceptance.

> [!IMPORTANT]
> Current status, 6 October 2026: the exact-main root build at `cc059003d` passed at 16:43:57 UTC and the exact installer entrypoint is running. Reviewed redesign preparation from `1fa14f33a` is now merged into main; its 71 offline checks do not prove rendering, and the merged tree has no native build result yet. Separately, both exact root build entrypoints and independent installer-byte verification passed for the earlier redesign candidate `a28944e3c14b2066ee63d14151c8aca23066d743`. Two authorized visible attempts exited before inspection, so there is no current screenshot or complete visual acceptance. Read the [current continuation](CLOSEOUT_PROMPT.md).

## Status paragraphs below the title

MCP and command-line automation is being implemented for live projects, headless
slicing, and configured printers. See [setup and verification status](docs/features/automation/mcp-and-cli.md).

The current interface work also includes [local personal wording](docs/features/windows/personal-vocabulary.md),
[separate slicing actions](docs/features/windows/print-actions.md), and
[searchable canvas menus](docs/features/canvas-menu-search.md). These changes are
under development for the combined delivery; the [audit register](docs/features/design-system/native-interface-audit-2026-10-02.md)
records historical runtime and layout evidence. Focused source and non-window geometry checks now cover the current redesign; they do not establish native runtime or screenshots.
Implementation is not a claim of verified physical-printer behavior.
Current compiler and startup evidence is summarized in the [continuation record](CLOSEOUT_PROMPT.md).
The earlier configure blockers are superseded. Current build and package evidence is recorded against exact source revisions in the verification report. The earlier production catalogue, documentation bundling and DeviceWeb observations belong to their dated source receipts. Reviewed inspection preparation covers all 55 declared states; 38 preparation checks and 33 native-review checks pass offline. The initial startup cause remains unknown, and a reviewed target-exit diagnostic prepares the next attempt. Exact-main packaging remains pending. The inherited DeviceWeb type stage attempted once and failed before bundling during the older delivery pass; no passing type-check result is claimed.
Earlier managed-check results apply only to their recorded source revisions. The combined source includes the reconciled continuation lanes and bounded repairs listed in the current handoff.

## Current development work (as of the rewrite)

The [Studio Atlas design](design/workflow-refresh.md) covers the whole native application and product-owned embedded views, retaining existing features and action semantics. It includes Prepare, Preview, Print, Monitor, settings, fields, dialogs and nested tools. Source implementation and independent review are ongoing; a shared palette alone does not complete a surface.

**The design can be reverted if the maintainer dislikes it.** Review the [change ledger](design/workflow-refresh/change-ledger.json) and reverse only the appropriate appearance changes, preserving build, printer, AMS, slicing and data fixes. Mixed functional/presentation commits require selective review; resetting to the old baseline is not the reversal procedure.

The previously preserved responsive-workflow and local-history source has been reconciled into the combined delivery revision. This pass adds bounded draft/history, continuity, import and automation repairs. Release delivery remains pending; see [the current continuation record](CLOSEOUT_PROMPT.md). Import cancellation remains cooperative and cannot interrupt an indivisible reader, hull calculation, object copy or final scene update. Issue #16's separately licensed companion remains a separate decision and is not included in this delivery.

The source branch for the next Windows release reconciles upstream Bambu Studio
2.8.4.57, with the fixes of upstream 2.8.4.61 merged on 2026-09-29, with this fork's native UI. It adds remembered dual-nozzle grouping,
quick swaps and a Slice and print setup action, LAN printer farm routing,
camera autoplay and fan feedback, a native Model Creator, portable multi-file
workspaces with checklists and planning, and history embedded in saved 3MF
files. The current combined source is undergoing local build and packaging; runtime and
printer verification are not performed in this pass. The latest published installer link above remains the
last verified release until a newer release is confirmed. See the
[roadmap](ROADMAP.md) and [handoff](HANDOFF.md) for exact state and limitations.
