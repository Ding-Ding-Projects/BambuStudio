# LAN printer farm

The multi-device picker, farm grid, send dialog, and task views use one inventory keyed by printer device ID. It combines account devices with discovered and saved local devices. A paired local device takes precedence when the same ID appears in both sources, because its address and access code are needed for direct printing. Remembered devices remain visible while offline. Selection is limited to six distinct IDs.

The picker leaves a dual-nozzle printer visible with an explanation that multi-device sending does not support it. LAN printers use the existing local pairing and access-code path. Before sending, the dialog checks for pairing, a local address, and a loaded networking module with direct LAN printing support. It explains the missing requirement and does not enqueue unusable work.

Direct LAN jobs carry the printer address, access code, username, and configured FTP and MQTT transport settings. The task manager serializes LAN transfers and checks cancellation before each transfer. Each task retains its own completion, cancellation, or failure state. Account-connected printers continue through the existing service route. A LAN-only printer never silently falls back to that route.

The standalone `farm_device_policy_tests` target exercises ID deduplication, offline inventory, the six-device selection cap, LAN readiness reasons, cancellation, serialized dispatch, and independent success and failure results with a simulated transport. It does not establish compatibility with physical printer firmware or network conditions. A real paired printer is required for that verification.
