# Automation and Home Assistant delivery scope

## Current delivery decision

The October 5, 2026 delivery pass starts from `bbfc86ae6`. Existing implementation
already supplies the requested MCP/CLI operations and both explicit Home Assistant
printer handover paths. This pass records their source boundaries rather than
recreating them. It makes no automation or Home Assistant source change.

Tests, lint, type checks, static-analysis suites, audits, runtime launches and
screenshots are deliberately skipped in this delivery pass. Earlier evidence
keeps its original source and scope. No new runtime or hardware result is claimed.

## Issue 53: MCP and CLI

| Requested behavior | Existing implementation |
| --- | --- |
| Shared typed command interface | `automation/BambuAutomation/AutomationTools.cs` declares 20 tools; `CommandService.cs` routes the same operations for MCP and CLI |
| stdio and authenticated Streamable HTTP | `automation/BambuAutomation/Program.cs`, `HttpSecurity.cs`, and `Options.cs` configure transports, bounded input, authentication, Host/Origin validation, and HTTPS for non-loopback listeners |
| Explicit native instance selection | `automation/BambuAutomation/NativeBridge.cs` discovers enabled current-user pipes; `CommandService.cs` rejects absent or ambiguous instances |
| Explicit workspace boundaries | `automation/BambuAutomation/Workspace.cs` and `src/slic3r/GUI/AutomationBridge.cpp` independently constrain paths and prevent unsafe traversal |
| Running-application operations | `src/slic3r/GUI/AutomationBridge.cpp` dispatches project, model, preset, settings, slicing, export and job operations |
| Headless slicing | `automation/BambuAutomation/SliceJobs.cs` owns bounded isolated jobs using the packaged native slicer; the one-shot CLI waits for its own job before exiting |
| Configured printer status/start/pause/resume/cancel | `src/slic3r/GUI/AutomationBridge.cpp` uses configured printer connections and readiness checks; credentials are not returned to callers |
| Idempotent print submission | `record_print_intent` in `AutomationBridge.cpp` durably records request identity before dispatch, with process result caching and explicit uncertain-submission handling |

The supported print-start boundary remains one known reliable nozzle, one used
filament, and an external spool, with matching printer model and nozzle diameter.
AMS and dual-nozzle starts are explicitly rejected. No arbitrary shell, raw printer
command or unrestricted G-code operation is exposed. Those restrictions are part
of the existing implementation, not missing code to silently remove.

The latest issue comments retain installed MCP/native interaction acceptance as
unverified. That is a verification gap. The delivery pass does not upgrade it to
verified behavior. Implementation delivery is eligible once the coordinating
task's required packaging, release and main-branch proofs are established; this
document alone is not that proof.

## Issue 16: Home Assistant printer handover

| Requested behavior | Existing implementation |
| --- | --- |
| Collect accessible configured printers without manual credential re-entry | `accessible_printers()` in `src/slic3r/GUI/SmartHomeDialog.cpp` selects accessible local/account records, deduplicates serials and collects the required handover fields |
| Path B, explicit service import | `SmartHomeDialog::add_printers_to_home_assistant` and `HomeAssistant.cpp` call `POST /api/services/bambu_lab/add_printer` in bounded four-wide waves |
| Credential transfer disclosure | The explicit import action displays the existing confirmation before sending printer access codes |
| Path A, temporary discovery | `SmartHomeDialog::set_discovery_sharing` and `HomeAssistantSharingService.cpp` provide off-by-default five-minute discovery, authenticated HTTP offers, expiration and shutdown |
| Secure bearer transport | `HomeAssistantTransportPolicy.cpp` restricts bearer requests to HTTPS or explicit loopback HTTP and disables credential-bearing redirects and verbose traces |
| Bounded background work | `HomeAssistantTaskExecutor.hpp`, `HomeAssistant.cpp` and the sharing service bound service admission, response sizes, import batches and discovery replies |

The handover is explicit because it transfers credentials. Opening Smart Home
does not enable sharing or submit printer data. This delivery pass does not
remove that boundary to interpret “automatically” as silent credential transfer.

The companion integration lives in a separate repository,
[`Ding-Ding-Projects/ha-bambulab`](https://github.com/Ding-Ding-Projects/ha-bambulab).
It is not modified in this lane. The recorded companion-license decision belongs
to that repository's owner; this pass neither invents a license nor asserts that
its external acceptance is resolved.

The latest substantive issue handoff lists real Home Assistant Path A/Path B,
native bilingual capture and physical-printer acceptance as outstanding. These
are skipped verification or external acceptance, not newly identified missing
Bambu Studio implementation. Issue 16 remains ineligible for an unrestricted
end-to-end completion claim until those separate criteria are satisfied or the
maintainer explicitly records a narrower implementation-delivery closure.

## Safety and remaining work

No configured printer or Home Assistant instance was contacted or changed. No
physical print was submitted, no user credential was read, and no new data
collection was introduced. No build, workflow, general widget, history, Plater or
Tab source was changed.

The coordinating task must record the new release and its actual source identity
separately. Packaging and release availability cannot serve as installed-runtime,
Home Assistant or physical-printer proof. For future verification, use the
existing [MCP documentation](../features/automation/mcp-and-cli.md),
[Smart Home contract](../features/windows/smart-home.md), and
[discovery API contract](../features/api/home-assistant-printer-discovery.md).
