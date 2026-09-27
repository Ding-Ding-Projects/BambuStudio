# HTTP/API features

This category documents network contracts served by the native Bambu Studio application. It does
not describe Home Assistant's own REST API or the bundled `DeviceWeb` webview.

## Contracts

- [Home Assistant printer discovery](home-assistant-printer-discovery.md) — a short-lived,
  bearer-authenticated LAN endpoint exposed only while the user enables discovery sharing.

## Postman collections

- [Category collection](postman/home-assistant-printer-discovery.postman_collection.json) — the
  Home Assistant discovery endpoint with contract checks.
- [Project master collection](../../postman/BambuStudio.postman_collection.json) — the project-level
  index containing every Bambu Studio-served HTTP contract currently documented.

The collections contain TEST-NET placeholders, never working credentials. Replace the host, port,
and pairing token only in a local Postman environment while a sharing window is active. Mark the
token secret, do not persist it into the collection, and remove the environment after verification.

No persistent or WAN-intended API is provided. Closing the Smart home dialog or disabling its
sharing toggle invalidates the only endpoint in this category.
