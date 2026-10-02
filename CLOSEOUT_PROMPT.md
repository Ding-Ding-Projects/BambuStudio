# MCP automation continuation

Objective: complete Bambu Studio MCP support under issue #53 and progress
Discussion #54. All product compilation, tests, packaging, installation,
slicing and runtime execution remain on GitHub-hosted Windows runners only.
No local product execution occurred.

The implementation branch is `feature/mcp-integration`. The verified remote
candidate before this handoff is `65dc4577fb29f2d00fd525a1de11e370784c5b3e`.
The default branch remains `ce883543177ef7df46fa5b798dcc5c7f3d2f8020`.
This handoff and documentation corrections are additional preservation work,
not completed default-branch integration.

Implemented: opt-in current-user native pipe bridge; project/model/preset/
settings control; generation-bound native slicing; atomic project and toolpath
export; configured-printer controls; durable print-intent replay protection;
20 typed MCP tools; stdio and authenticated Streamable HTTP; shared JSON CLI;
isolated headless slicing; self-contained companion packaging; exact source
and payload hash checks; encrypted hosted runtime evidence producer and reader.
The corrected command reference describes actual native job cancellation and
durable print-intent behavior.

Hosted run 37045639569 at the exact candidate passed all 24 managed checks,
zero failures and zero skipped cases: BoundaryTests.cs has 22 cases and
TransportTests.cs has 2. Native compilation, payload validation, companion
staging, Squirrel creation and package validation passed in job 110966508477.
The receipt is docs/features/automation/hosted-verification.json.

The run reached its agreed one-hour observation limit at 2026-10-02T19:10:22Z.
Last observation at 19:10:35Z: release job 110986759894 remained in progress at
"Validate Squirrel release assets and prepare metadata". Observation stopped;
the hosted job was not canceled. A structured request to extend this current
run to two hours has no answer yet. No release completion is claimed.

Remaining: obtain the terminal release result after an authorized extension
or later terminal-state handoff; identify the release targeting the exact
candidate; dispatch mcp-runtime.yml with that tag and expected_source_commit;
verify the installed package, native project round-trip, native slicing and
headless slicing; decrypt and inspect restricted genuine captures; update
factual evidence; integrate completed work into main and verify its remote
SHA; archive and remove only proven task-owned redundant branches/worktrees.
Do not merge unfinished work merely to satisfy a default-branch requirement.

Physical printers remain unverified. The supported start mapping is one
reliable known nozzle and one external-spool filament. AMS and dual-nozzle
starts are explicitly unsupported. Never replay an uncertain print submission.
No arbitrary shell or raw printer command interface is exposed.

Only the dedicated public evidence recipient is tracked. Its protected private
key remains outside version control. No runtime capture exists yet. Preserve
all task worktrees, branches and local evidence until verification and archive
proof are complete. No cleanup or deletion has run. Project-board access lacks
read:project; authenticated shared-status enrollment is unavailable. The wiki
uses its host-required master branch. No unrelated backlog was adopted.

Wiki handoff: `f0dc140038ae19d0487f74a4b4ba0a64b265485b` on master, remote verified.
