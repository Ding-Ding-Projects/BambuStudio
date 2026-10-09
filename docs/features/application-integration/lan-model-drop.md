# LAN model drop

People on your local network can send a 3D model to the Bambu Studio on this computer from a phone
or a laptop browser. A small site runs in a Docker container (the `lan-model-drop` service in the
`lan-model-drop/` folder of this repository); Bambu Studio is the station that collects what
arrives. Every model waits for you: nothing is opened, sliced or printed until you choose Open.

The option is off by default.

## Where to find it

- **Preferences, LAN model drop.** A section of its own in the Preferences strip, after Other. It
  holds the switch **Receive models from the LAN drop site**, the invite, and the connection rows.
- **File, LAN model drop...** opens that section on the switch.
- **File, Invite someone to send a model...** opens the invite dialog.
- **Command palette (Ctrl+Shift+F).** "Invite someone to send a model", "LAN model drop settings"
  and every row of the section ("Receive models from the LAN drop site", "Invite link and QR code",
  "Test connection", "Drop site address", "Station key", "Drop code") land on the exact row.
- **Settings search** in Preferences finds the same rows.
- **Title bar.** While the option is on, a LAN indicator sits left of the notification bell. It is
  Primary while connected, Error on a problem and neutral while connecting, carries the number of
  received files waiting as a badge, and opens the invite dialog. Its tooltip is its accessible
  name and states the status.

## Setting it up

1. Start the drop site on a computer of your network (usually this one) as its README describes,
   for example `docker compose up -d` in `lan-model-drop/`. It publishes port 8833 by default.
2. Read its station key on that computer:
   `docker compose exec lan-model-drop node server/station-key.mjs`.
3. In Bambu Studio open **File, LAN model drop...**, switch on **Receive models from the LAN drop
   site**, paste the key into **Station key** and press **Save key** (or Enter).
4. Leave **Drop site address** at `http://localhost:8833` when the container runs on this computer,
   or enter the address Bambu Studio reaches it at, such as `http://my-nas.local:8833`. The field
   accepts `http://` or `https://`, a host name or an IPv4 address and an optional port; anything
   else is refused with an explanation and not saved.
5. **Test connection** checks `/healthz` and then the station status with the key. The status line
   reads Connected (with the station name), Not reachable, Wrong station key, Protocol not
   supported, Station key needed, Address not valid or Protected key storage unavailable.

## The invite

The invite is what you hand to the person who sends the model. It appears in the Preferences
section right under the switch, so it is the first thing shown once the option is on, and in the
dialog **Invite someone to send a model**, which offers **Turn on LAN model drop** while the option
is off and then moves focus to **Copy link** as soon as the link exists.

- **The link** is `<base>/#code=<drop code>`, shown as selectable text. The drop code travels in the
  fragment, so it reaches neither server logs nor a Referer header; the site reads it, fills the
  code field and removes the fragment from the address bar.
- **The base**, in this order: the container's public URL when it reports one (`publicUrl` from
  `DROP_PUBLIC_URL`, for a reverse proxy or a fixed name); otherwise the configured address when it
  is not a loopback address; otherwise `http://<this computer's private LAN IPv4>:<port>` with the
  configured scheme and port. When this computer has several private IPv4 addresses (10/8,
  172.16/12, 192.168/16), **Address of this computer** lets you pick one; the choice is remembered.
  Loopback, link-local, multicast and public addresses are never offered. A line under the link says
  which base is in use. With no usable base the invite says so instead of showing a link.
- **Copy link** puts exactly that link on the clipboard and announces "Link copied to the
  clipboard." to screen readers.
- **The QR code** encodes exactly the same link with the bundled encoder (the one the local security
  pairing uses), black on white with a four-module quiet zone in both themes. Its accessible name is
  "QR code of the invite link". **Show larger QR code**, or a click on the code, opens a large view
  for a phone held further away.
- **New link** asks the container for a new drop code (`POST /api/station/drop-code`), so links with
  the old code stop working. When `DROP_CODE` fixes the code the button is disabled and the invite
  explains how to change it on the drop site.
- The link appears once Bambu Studio has the current drop code from the station status; until then
  the invite explains what it is waiting for.

The **Drop code** row shows the same code with a **New code** button.

## When a model arrives

While the option is on, Bambu Studio checks the drop site every 5 seconds. After an error it waits
10, 20, 40 and then 60 seconds between attempts, and returns to 5 seconds once it reaches the site.

For each new file it downloads the bytes, then checks that the size equals the size the site
listed, that the SHA-256 digest equals the listed digest (and the `X-Content-SHA256` header when
present), and that the name keeps an accepted extension that matches the listed type and the
content (3MF, STL, STEP, OBJ, AMF). The file is saved under the data folder in
`lan-model-drop/received/<id>/<file name>`; the name is reduced to a safe base name first.

A notification then reads "<sender> sent <file> (<size>)", or "Someone sent <file> (<size>)", with
two links:

- **Open** loads the file through the same path as a file dropped on the window, so the usual
  questions about an unsaved project or opening a 3MF as a project still apply. Bambu Studio then
  asks the drop site to delete its copy.
- **Discard** removes the local copy and asks the drop site to delete its copy.

Closing the notification leaves the file waiting. The invite shows how many files are waiting with
**Show them**, which shows their notifications again. A file that fails a check is refused with a
warning that says why, and is deleted from the drop site. A file that cannot be downloaded after
three tries stays on the drop site until it expires; switching the option off and on tries again.
Files that were never opened are removed from the data folder at the next start and arrive again
if the drop site still holds them. An opened file keeps its folder, because the project may point at
it; use Save Project as to keep a project somewhere else.

## Security and privacy

- The station key is a secret. It is stored only as a Windows DPAPI blob for the current Windows
  account (`CryptProtectData` with purpose entropy, user interface forbidden) in
  `lan-model-drop/station-key.dpapi` in the data folder, never in the settings file. Another
  account, or a copy of the folder on another computer, cannot decrypt it. Other systems report
  the storage as unavailable instead of keeping the key in plain text.
- The key is never logged. Requests carry only `Authorization: Bearer <key>` and `Accept`: the
  application's usual extra headers are removed, redirects are not followed and the transfer trace
  is off. Copies of the key in memory are overwritten after use. The masked field can be shown with
  the eye button; both fields are excluded from captures, history and exports.
- All network work runs on one worker thread. Results reach windows only through the event queue
  and only while the main window exists. Shutdown stops the worker before windows are destroyed: it
  sets the cancel flag, aborts a transfer in flight, wakes the worker and waits for it.
- Nothing is opened, sliced or printed automatically. There is no analytics or telemetry.

## Settings

| Key | Meaning |
| --- | --- |
| `lan_drop_enabled` | The switch; off unless `true` |
| `lan_drop_address` | The drop site address, default `http://localhost:8833` |
| `lan_drop_invite_ipv4` | The private LAN address chosen for the invite link |

## Verification

- `node --test tests/lan_model_drop/model_native.test.mjs` builds
  `tests/lan_model_drop/lan_model_drop_model_test.cpp` with `g++ -std=c++17 -Wall -Wextra -Werror`
  against the wx-free model and runs it: protocol parsing and validation, accepted types and content,
  file and sender names, the backoff schedule, status classification, addresses, the invite base and
  link, and the inbox tracker.
- `node --test tests/lan_model_drop/app_wiring.test.mjs` reads the sources and checks the visible
  section, the menu items, the palette rows, the indicator, DPAPI storage, that the key is never
  logged or saved in the settings, that every request runs on the worker thread and reaches windows
  through the event queue, the shutdown order and that nothing is opened automatically. Every
  contract also runs against a mutated source to prove it can fail.
- The station sources compile in a Linux syntax check; their Windows-only parts (DPAPI, adapter
  enumeration, accessibility events) have only been reviewed. A built Windows application talking
  to a real drop site on a real network has not been observed yet.
