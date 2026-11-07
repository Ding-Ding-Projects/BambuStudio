# Start a local model drop server

The local hosting entrypoint pulls the versioned container image, starts the existing
Compose project and checks application health. It preserves the `drop-data` volume,
queued models and station identity. It never starts slicing or printing.

## Start from the command line

With Docker Desktop running, use `host.bat --detached` at the repository root.
Double-clicking `host.bat` offers a keyboard selector for background mode or live logs.
`host.bat --logs` adds a status view: F follows, P pauses, R refreshes, and Q or Escape
leaves the containers running. There is one selectable service, `lan-model-drop`.

The script prints private-network URLs that answered the health request from the
hosting computer. Check the chosen URL from the sending device separately. A response
on the host does not prove inbound firewall access from another device. The script
does not change firewall rules, operating-system settings or unrelated containers.

## Connect from Bambu Studio

The native setup integration adds **Start local drop server** and **Open drop page**
to the invite surface, reachable from File and Preferences. Automatic pairing uses
a current-account-only local named pipe and protected station storage. It does not
print the station credential or replace a configured remote station. Settings changed
while setup runs take precedence over the pending result.

## Recovery and development

Missing Docker, unavailable Compose, a failed image pull, occupied ports, failed
health checks and unavailable LAN addresses return nonzero with a bounded status
code. Fix the named condition and retry. Stopping the service is a separate explicit
`docker compose -f lan-model-drop/compose.yaml stop` operation. Do not remove its volume
to repair a connection problem.

The normal Compose file uses the published version. Developers can explicitly use
`docker compose -f lan-model-drop/compose.yaml -f lan-model-drop/compose.build.yaml up -d --build`
for a local build. This is not proof of published-image delivery.

## Current verification state

Source preparation is in progress. The PowerShell parser and root Compose resolution
pass. The real detached entrypoint reports unavailable runtime and unavailable image
without starting a container. Native transport fixtures cover the restricted pipe,
bounded input and cancellation. Full application compilation, live setup, cross-device
LAN access, interactive log input, image publication and installed release proof remain
pending. No portable setup companion has been delivered by this change yet.
