# Local printer status API

Bambu Studio can serve a small, read-only web interface on the computer it runs on, so other programs on that computer (a home-automation script, a stream overlay, a shell prompt) can ask which printers the application knows and whether a printer's door is open. It is off by default.

Native runtime behaviour has not been verified. The routing, authorization and JSON rules are exercised by a header-level test; the listener, the Preferences section and the snapshot timer have not been built or driven in a running application, and the door reading itself is a beta reading that has not been checked against a real printer (see [Live printer readings](live-readings.md)).

## Turning it on

Open Preferences and choose **Local status API**.

| Setting | Config key | Default | Notes |
| --- | --- | --- | --- |
| Serve printer status on this computer | `status_api_enabled` | off | Starting and stopping the listener happens at once, without a restart. |
| Port | `status_api_port` | 18787 | A whole number from 1024 to 65535. Anything else falls back to 18787. A port that is already taken shows "The port is already in use". |
| Access token | `status_api_token` | made on first switch-on | 64 hexadecimal characters from the OpenSSL random generator. Shown masked, with Show, Copy token and Regenerate token. |

Regenerating the token stops the old one at once, without restarting the listener. The token is kept in the application settings file like other preferences. It is never written to a log, and the token row is registered with the sensitive-surface registry that the other secret rows use.

## Security model

- **Loopback only.** The listener binds `127.0.0.1`. There is no setting that widens it. A connection whose peer address is not loopback is dropped, and the request handler refuses it again with 403.
- **Host check.** The `Host` header must be `localhost`, `127.0.0.1` or `[::1]`, with an optional port. A web page on another origin that points its own name at `127.0.0.1` sends a different `Host` and gets 403.
- **Token required.** Every endpoint needs `Authorization: Bearer <token>`. The token is compared in constant time. A missing, malformed or wrong token gives 401 with `WWW-Authenticate: Bearer`. A switched-off API or an unusable token never accepts a request.
- **GET only.** Every other method, including `HEAD` and `OPTIONS`, gives 405 with `Allow: GET`. No response carries a CORS header, so a browser page cannot read the API even with the token.
- **Small and bounded.** The request line and headers together may not exceed 8192 bytes, a request body is not accepted, the target may not exceed 256 characters, and a connection that does not finish its request within five seconds is closed. One request is served per connection.
- **Read-only.** Nothing in the API changes a printer or a setting.
- **Plain HTTP.** The traffic never leaves the computer, so it is not encrypted. Do not forward the port to another machine.

The checks run in this order: enabled, peer, Host, method, target length, token, route. A request that is not from this computer or not a GET is refused before the token is looked at.

## Endpoints

All responses are UTF-8 JSON with `Cache-Control: no-store`. Errors have the form `{"error": "<code>", "message": "<text>"}`.

### `GET /api/v1/health`

```json
{"status":"ok","api_version":1}
```

### `GET /api/v1/printers`

The printers the application lists under My devices.

```json
{"printers":[{"dev_id":"01P00A000000000","name":"Workshop","model":"H2D","connected":true,"printing":false}]}
```

| Field | Meaning |
| --- | --- |
| `dev_id` | The printer serial number used in the other endpoints. |
| `name` | The name shown in the application. |
| `model` | The model text shown in the application. |
| `connected` | The application currently has a connection to the printer. |
| `printing` | The printer is in a printing state. |

### `GET /api/v1/printers/{dev_id}/door`

```json
{"dev_id":"01P00A000000000","supported":true,"state":"closed","source":"home_flag bit 23 or print.stat bit 23","beta":true,"updated_at":"2026-11-04T09:30:12Z"}
```

| Field | Meaning |
| --- | --- |
| `supported` | The printer reports door-open detection support (`MachineObject::support_door_open_check()`). |
| `state` | `open`, `closed` or `unknown`. `unknown` is returned when the printer has no door sensor, or the application has not received bit 23 of `home_flag` or `print.stat` (`MachineObject::door_open_bit` is -1). It is never guessed. |
| `source` | Where the bit comes from. That position is from community protocol notes, not a vendor document. |
| `beta` | Always `true` for now. |
| `updated_at` | ISO 8601 UTC time at which the application last saw this printer's state change (or first saw it). It is not the time of the last status message. |

Responses: `404` with `printer_not_found` for an unknown or malformed `dev_id`, `401` without a valid token.

## How it reads printer state safely

Printer objects belong to the GUI thread. While the API is on, a timer on the GUI thread copies the few fields it needs (id, name, model, connection, printing, door support and door bit) into a snapshot every second. The server thread reads only that snapshot, under a mutex, and never touches a printer object. The values are therefore up to about one second old. The snapshot is cleared when the API is switched off.

## Failure modes

- Port in use: the state row says so and nothing listens; change the port.
- No token: the state row says so; use Regenerate token.
- The application is closing: the listener and the timer are stopped before windows are destroyed.
- No printers: `/api/v1/printers` returns an empty list.

## Verification

- `node tests/status_api.test.mjs` compiles the standard-library header `src/slic3r/GUI/StatusApi/StatusApiCore.hpp` and checks routing, authorization, the door mapping and the JSON, and pins the wiring (loopback bind, token required, off by default, GET only). `--negative` removes each guard in turn and confirms the test fails.
- Not verified: building the application with the new files, the listener on a real socket, the Preferences section, and any real printer.

A Postman collection for these endpoints is in [status-api.postman_collection.json](status-api.postman_collection.json), and it is included in the master collection at `docs/postman/BambuStudio.postman_collection.json`.
