# Bambu Studio automation service

The `bambu-automation.exe` companion is a self-contained Windows x64 .NET 10 executable. It exposes the same command service through the official MCP C# SDK (pinned `ModelContextProtocol.AspNetCore` 2.2.0), standard input/output, Streamable HTTP at `/mcp`, and a JSON CLI. It connects to the actual Bambu Studio GUI through a current-user named pipe; it does not implement a second printer protocol.

## Package layout and build

Publish alongside the packaged native executable:

```text
bambu-studio.exe
automation/
  bambu-automation.exe
  README.md
```

```powershell
dotnet publish automation/BambuAutomation/BambuAutomation.csproj -c Release -r win-x64 --self-contained true -o package/automation
dotnet test automation/BambuAutomation.Tests/BambuAutomation.Tests.csproj -c Release
```

Compilation, automated checks, installation, runtime checks, and slicing are executed only on the approved hosted build machine for this task. Source written here is unverified until that machine reports a verdict. The test project includes workspace and HTTP security checks, actual compiled MCP stdio/HTTP initialization, discovery and tool-call exchanges, real local named-pipe framing checks with a fixture server, and simulated printer identity/replay checks. Those fixture results do not prove a real printer started or native readiness checks passed.

## Enable native automation explicitly

Before launching Bambu Studio, set `BAMBU_AUTOMATION=1` and `BAMBU_AUTOMATION_ROOTS` to a semicolon-separated list of absolute existing workspace directories. The native bridge is otherwise disabled. Set the companion's `--workspace` arguments explicitly to the same roots. Each enabled instance listens on `BambuStudio.Automation.v1.<PID>` using a current-user byte-mode pipe.

```powershell
automation/bambu-automation.exe serve --workspace C:/PrintWorkspace
automation/bambu-automation.exe command instances --workspace C:/PrintWorkspace --json
automation/bambu-automation.exe command project_inspect --instance 1234 --workspace C:/PrintWorkspace --json
automation/bambu-automation.exe command project_open --instance 1234 --workspace C:/PrintWorkspace --arguments @C:/PrintWorkspace/open.json --json
```

`--arguments-stdin` reads the JSON object from standard input. `--arguments` also accepts an inline JSON object or `@` followed by an absolute workspace file. One-shot commands emit one JSON line, with `{ "ok": true, "result": { ... } }` and exit 0, or `{ "ok": false, "error": { "code": "...", "message": "..." } }` and exit 1. Diagnostics go to stderr. MCP stdout is exclusively protocol traffic.

Multiple enabled instances require an explicit `instanceId` in tool arguments or `--instance` on the CLI. File operations accept absolute paths within workspace roots only; traversal, sibling-prefix escapes, network/device paths, alternate data streams, symbolic links, and reparse points are rejected. Output parent directories must exist. Replacing an existing file requires `overwrite: true`. Workspace directories must be trusted and access-controlled: these checks do not authorize another local user to mutate workspace ancestry concurrently.

## Operations

Every operation has a stable `bambu_<operation>` MCP tool accepting an `arguments` object with an operation-specific typed schema, required fields, and constraint descriptions. The CLI uses the operation name without the prefix.

| Operation | Required arguments and behavior |
| --- | --- |
| `instances` | List enabled native instance process IDs. |
| `capabilities` | Service transports and headless availability are reported even with no GUI attached. Selected native capabilities appear in `nativeCapabilities`; ambiguous GUI instances report selection required. |
| `project_inspect`, `project_new` | Inspect or create a native project. |
| `project_open`, `model_import` | `path` to a workspace file. |
| `project_save` | `path`, optional `overwrite`. |
| `presets_list`, `settings_get` | Read actual native presets/settings. |
| `settings_update` | `values` object; native validation remains authoritative. |
| `slice_start` | Native slicing by default; headless details below. |
| `export_file` | `path`, `format` accepted by native bridge, optional `overwrite`. |
| `printer_list` | Only printers connected through the selected native instance. |
| `printer_status`, `printer_pause`, `printer_resume`, `printer_cancel` | Explicit `printerId`. |
| `printer_start` | Explicit `printerId`, `requestId`, new `.3mf` staging `path`, `overwrite:false`, `useAms:false`, `amsMapping:[-1]`, `nozzleMapping:{}`. Native exports the current sliced plate itself, checks one known reliable nozzle, one used filament, matching target model/diameter, online/idle state, and sliced-plate readiness. No additional confirmation dialogue is introduced. Request-ID deduplication lasts only for the native process lifetime. Never automatically replay after restart, `operation_in_progress`, or `submission_unknown`. |
| `job_status`, `job_cancel` | `jobId`; native jobs remain with their native instance. Native slicing may report `result_available` with `completionVerified:false`; native cancellation returns `cancellation_not_safe` without a safe native operation ID. Those states are not converted into verified completion or successful cancellation. |

The service never accepts shell commands, executable paths, raw printer instructions, or caller-supplied native CLI flags. Unsupported operations and disconnected printers return structured errors rather than simulated success. A timeout or disconnect leaves mutation outcome uncertain; query state and reuse the same print `requestId` before retrying.

## Isolated headless slicing

`slice_start` with `headless: true` requires a configured `.3mf` input in `path`, a distinct `.3mf` `output`, optional integer `plate` (0 means all plates), and optional `overwrite`. The packaged native CLI uses `--slice`, `--export-3mf`, `--datadir`, and `--outputdir`; native `--export-gcode` is disabled in this source. Each job uses an isolated directory under the output parent's `.bambu-automation-jobs/` directory. Input is snapshotted while held against concurrent writes/deletion before queue admission; `inputSha256` records that snapshot's identity. Active jobs cannot share an output destination. The service accepts at most four active jobs, executes one at a time, retains at most 128 job records, and limits native execution to 30 minutes. Diagnostics are drained without retaining or reflecting potentially sensitive native log text.

The service reports progress as indeterminate until native completion, then 100%. Exit 0 alone is insufficient: the exported ZIP must exist and contain nonempty, readable `.gcode` entries. Successful output includes the actual path, byte count, SHA-256, native exit code, and job ID. Failed job directories are retained for inspection. They are not automatically deleted.

MCP server mode returns an asynchronous `headless-...` job ID for polling/cancellation. One-shot CLI mode waits for that job because its in-memory registry ends with the process; Ctrl+C cancels the owned process tree. Native slicing jobs can still be polled using the owning GUI instance.

## HTTP authentication and TLS

```powershell
automation/bambu-automation.exe serve --transport http --listen http://127.0.0.1:8766 --workspace C:/PrintWorkspace --bearer-file C:/Protected/automation-bearer
```

HTTP always requires bearer authorization, including loopback. Use a randomly generated secret in an owner-protected file or `--bearer-stdin` (read through EOF); a secret option on the command line is deliberately unsupported. On Windows the file must be owned by the current user and readable only by that user, SYSTEM, or administrators. Do not place the file in a repository, diagnostic log, client URL, or screenshot. An HTTP client sends `Authorization: Bearer <value>` through its protected configuration.

The default binds only `127.0.0.1`. Host must match the configured authority exactly. Browser Origin, when present, must match the configured origin. There is no permissive CORS configuration. Bearer comparison uses fixed-time comparison of fixed-size SHA-256 digests, and malformed/oversized authorization headers are rejected.

Non-loopback listeners require HTTPS. Import a valid server certificate with its private key into the service account's `CurrentUser/My` certificate store using an owner-managed protected route, then select it with `--certificate-thumbprint`. The thumbprint identifies the public server certificate, not a secret. The selected certificate must be valid, unique, and contain a private key. Its name/IP coverage and client trust must match the listener. Example listener options are `--listen https://192.168.1.20:8766 --certificate-thumbprint <public-thumbprint>`. Do not disable client certificate validation. The listener host must be a literal local IP address or `localhost`; wildcard and DNS rebinding routes are not enabled.

HTTP bodies and pipe frames are limited to 1 MiB. Native requests are versioned, correlated newline-delimited UTF-8 JSON:

```json
{"version":1,"id":"request-id","operation":"project_inspect","arguments":{}}
```

Native success contains `version`, matching `id`, `ok:true`, and an object `result`; native failure contains `ok:false` and an object `error` with `code` and `message`. The companion verifies response version, identity, framing, and size. MCP tools return both readable JSON content and structured content, with `isError:true` for command errors.
