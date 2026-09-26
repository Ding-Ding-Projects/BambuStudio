# LAN farm sending and device eligibility

The farm list combines account and locally discovered printers by device ID. A paired local instance takes precedence when both sources report the same ID. Saved local credentials and addresses allow a previously paired printer to remain listed while offline. Selection persists up to six distinct device IDs.

The multi-device sender now carries the same `nozzle_mapping` and `nozzles_info` fields as the single-printer path when it can validate them. Material `nozzleId` follows the current plate's effective left/right assignment. Fixed dual-nozzle printers require a current plate with complete assignments and a matching two-nozzle preset. A printer with a dynamic nozzle rack additionally needs a current device mapping result that resolves every used material. The payload is checked before export and again before task creation.

An incomplete plate assignment, missing nozzle preset, invalid diameter or flow type, stale rack result, or unsupported multi-plate selection receives a specific reason. These cases are not queued. The picker keeps such printers visible and selectable because a refreshed mapping may make them eligible; the final send action remains the deciding check. The single-printer flow remains available when the farm path cannot prove compatibility.

An eligible paired LAN printer uses its local address and access code through the authenticated local-send function. Missing pairing, address, or local transport receives a specific reason before export. LAN transfers are serialized because the local networking module permits one transfer session at a time. A waiting task checks cancellation before entering that session; a task canceled while a transfer finishes is recorded as canceled rather than completed. Individual task outcomes remain visible in the task list.

The focused policy regression covers duplicate inventory, the six-device limit, LAN readiness, plate nozzle-map validity, cancellation-aware completion, and serialized local dispatch. A native build, UI drive, real dual-nozzle LAN printer, and transfer cancellation remain unverified until they run through the project's release verification route.
