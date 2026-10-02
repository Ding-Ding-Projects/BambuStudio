# MCP and command-line automation

## Delivery and status

The Windows payload includes `automation/bambu-automation.exe`, a self-contained
companion. It provides stdio and Streamable HTTP MCP transports and a CLI using
the same validated command service. No separate .NET runtime is required.

Implementation and verification are tracked in [issue #53](https://github.com/Ding-Ding-Projects/BambuStudio/issues/53).
Source availability is not packaged-runtime proof. The focused hosted workflow
checks the managed service; the separate release-runtime workflow checks the
installed native application. Real printer verification remains separate.

## Enable an application instance

Set `BAMBU_AUTOMATION=1` and `BAMBU_AUTOMATION_ROOTS` before launching the application.
The roots value is a semicolon-separated list of explicitly allowed directories.
Leave the switch unset for normal use with no native automation listener.

The native bridge accepts only same-user local named-pipe clients. Each enabled
process has its own `BambuStudio.Automation.v1.<PID>` endpoint. Select an instance
explicitly when more than one is running. The companion checks workspace access
before forwarding requests, and the native bridge independently checks its roots.

## Connect a local MCP client

Configure the MCP client to launch the installed companion with:

```text
serve --transport stdio --workspace C:\Models
```

Use the actual absolute installed executable path in the client's `command`
field. Pass each option as a separate argument. Standard output contains only
protocol messages; diagnostics use standard error. Client configuration does not
enable a native instance by itself.

## Streamable HTTP

Use `serve --transport http` with the authentication and endpoint options in the
[command reference](../../../automation/README.md). The default is loopback-only.
Non-loopback access requires HTTPS, authentication, and an explicit network
configuration. Origin and Host checks protect the endpoint from browser-based
cross-origin and rebinding requests. Do not put credentials in command arguments,
URLs, configuration examples, or logs.

This version supports clients that can supply a configured bearer credential.
It does not implement browser account enrollment or an OAuth authorization server.

## CLI fallback

The command interface uses the same operations as MCP:

```text
bambu-automation.exe command capabilities --arguments "{}" --json --workspace C:\Models
```

Use a JSON file or standard input for complex arguments as documented in the
command reference. The CLI is a transport fallback, not a separate slicing
implementation. Native-instance commands still require the enabled application;
headless jobs use the packaged slicer in isolated directories.

## Jobs and printer actions

Project inspection, import, settings, slicing, export, job monitoring, and
configured-printer commands are exposed through capability discovery. Unsupported
device features return explicit errors rather than fabricated success.

An explicit print-start call does not add a second confirmation dialog. It must
identify the printer, request ID, completed native `sliceJobId`, and required print configuration. Native
readiness and supported mapping checks still apply. Retrying a request must not
start another physical print. An uncertain network result is reported as uncertain
and must be reconciled with printer state before a new request is submitted.

Native slicing and export accept an optional zero-based `plateIndex`; omitted
or null selects the current plate. Inspect the plate inventory before choosing
an index. Headless slicing retains its separate `plate` convention: zero means
all plates and a positive number selects that native CLI plate number.
Model export supports binary STL for supported FFF plate geometry. Negative
volumes requiring an interactive boolean choice are rejected. Project saving
uses `.3mf`; printable export uses a sliced `.3mf` archive.

Printer operations reuse the application's configured connection. Credentials
are never returned to the MCP client. No arbitrary shell, raw printer command,
or unrestricted G-code execution tool is provided.

## Failure and privacy boundaries

Files must resolve inside configured workspace roots. Reparse traversal and
implicit overwrites are rejected. Unsaved projects are not silently discarded.
Long operations have bounded job state, cancellation, and explicit results.
Transport disconnection is not proof that a physical printer command was undone.

Verification uses public model fixtures and simulated printers unless authorized
hardware is explicitly available. No local build, test, installation, or runtime
verification is performed for this implementation task. Hosted evidence records
the exact source and packaged binary identities.

## Related documentation

- [Command reference](../../../automation/README.md)
- [Automation index](README.md)
- [Release supply chain](../releases/windows-release-supply-chain.md)
