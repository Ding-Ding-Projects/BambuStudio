# LAN model drop site

## Direct sending update (1.0.1)

Mixed-version compatibility: protocol-1 status includes directSend:true and an inert dropCode value of not-required for older receivers. The authenticated legacy renewal endpoint returns 410 retired, never a successful rotation or revocation. An older desktop may briefly show a connection error after trying renewal; a later status poll can restore collection, but an in-flight poll can still trigger misleading renewal wording in that older binary; update the desktop to remove the obsolete control. The inert status value is never an upload credential. New receivers keep collecting from older services but hide direct invite links until the service advertises the capability, and show an upgrade instruction. Older desktop versions retain their old code labels until updated.

Invite links contain only the address, with no credentials or query parameters. Code-renewal controls are removed. Legacy DROP_CODE configuration and the existing drop-code file are ignored without deleting either. Upgrades retain the same volume and waiting models. Model-format, size, queue and expiry limits remain enforced.

Anyone who can reach the service can send, and the station needs no key: inbox access, downloads and removal are open to anyone on the network, while the computer running Bambu Studio shows every model and asks whether to open or discard it. An Authorization header sent by an older Bambu Studio is ignored. Each source address allows 60 upload attempts per minute, followed by a five-minute cooldown. Use a trusted local network and never expose the service to the internet. Container 1.0.1 was published successfully by [run 42418444721](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/42418444721) from source `601003f9b97d8945732d5a166f8dbe2b061d040f`. The verified image digest is `sha256:9382599181a32a5d78730c82b315612fcadc51beacea0ad3b831500e122ce51a`; build-context SHA-256 is `4fb31a53d02699892ba4669a012900b13b7549040bba6e9d1f8cbf3a4424a389`. The exact digest was pulled and an isolated Compose project passed `up --wait`, local health and second-host LAN health checks. A real browser sent a synthetic STL with HTTP 201 and no sender-code header. Six desktop/mobile language tuples passed code-field absence, named accessibility controls, keyboard focus and horizontal-overflow checks. Edge reported an ignored Cross-Origin-Opener-Policy header on the HTTP LAN origin, so the zero-console-error criterion remains unmet. The existing service was then updated through host.bat --detached: its original station key still authenticated, its data volume and queued models were preserved, and health passed locally and from a second LAN host. Native desktop acceptance remains separate.

The LAN model drop site is a small web page that runs in a Docker container
on your local network. Other people open it on a phone, tablet or laptop and
send 3D models to Bambu Studio MD3 on your computer. Bambu Studio collects
each model, shows who sent it and asks you to open or discard it. Nothing is
opened, sliced or printed by itself.

This article covers the container: starting it, reaching it from other
devices, settings, updates and security. In Bambu Studio the
receiving side lives in **Preferences > LAN model drop** and **File > LAN
model drop...**, which also make the invite link and QR code you hand to the
other person.

## What you need

- Docker Desktop for Windows on this computer, or any Docker host on the same
  network (a home server, a NAS that runs containers, a mini PC). The image
  runs on 64-bit Intel, AMD and ARM machines.
- Bambu Studio MD3 on this computer.
- The other person on the same network as the container: the same Wi-Fi or
  wired network. Guest Wi-Fi networks often keep devices apart, so a phone on
  a guest network may not reach the page.

## Start it on this computer with Docker Desktop

The Windows installation places a `lan-model-drop` folder beside the
application, in `%LOCALAPPDATA%\BambuStudioMD3\app-<version>\lan-model-drop`.

1. Start Docker Desktop and wait until it reports that the engine is running.
2. Open PowerShell in the folder:

   ```powershell
   cd "$env:LOCALAPPDATA\BambuStudioMD3"
   cd (Get-ChildItem -Directory app-* | Sort-Object LastWriteTime | Select-Object -Last 1).FullName
   cd lan-model-drop
   ```

3. Start the container:

   ```powershell
   ../host.bat --detached
   ```

   The launcher pulls the published version, starts it without rebuilding, and
   checks health and LAN addresses. Image publication, direct browser sending and the existing-service upgrade are verified; a failed pull exits without claiming success.
   Use `../host.bat --logs` for the interactive status and log view.
4. Check that it is healthy:

   ```powershell
   docker compose ps
   ```

   The status reads `healthy` within about half a minute. Open
   `http://localhost:8833` in a browser on this computer to see the page.

The container starts again by itself when Docker Desktop starts, until you
stop it (see [Stop it](#stop-it)).

### Open the Windows firewall for the port

Other devices reach the page through port 8833 of this computer. Windows
Firewall blocks that port until you allow it. Run this once in PowerShell
opened with **Run as administrator**:

```powershell
New-NetFirewallRule -DisplayName "Bambu Studio LAN model drop" -Direction Inbound -Protocol TCP -LocalPort 8833 -Action Allow -Profile Private
```

The rule applies only to networks Windows treats as private. Check that your
home or office network is set to **Private network** in **Settings > Network
& internet > Wi-Fi** (or **Ethernet**) **> the network's properties**. Never
allow the port on public networks. To remove the rule later:

```powershell
Remove-NetFirewallRule -DisplayName "Bambu Studio LAN model drop"
```

If Windows shows its own firewall prompt for Docker Desktop, allow it for
private networks only.

### Find this computer's LAN address

Bambu Studio shows the address people should open, as a link and a QR code,
once it is connected, so usually you do not need to look it up. To find it
yourself:

```powershell
Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.PrefixOrigin -in 'Dhcp','Manual' } | Select-Object InterfaceAlias, IPAddress
```

Use the address of the adapter that is on the shared network (Wi-Fi or
Ethernet), not one that belongs to a virtual adapter such as `vEthernet
(WSL)`. Home networks usually hand out private addresses that begin with
192.168 or 10. People then open `http://<that address>:8833`.

## Connect Bambu Studio

1. In Bambu Studio open **Preferences > LAN model drop**, keep the site
   address `http://localhost:8833` and choose **Test connection**. The status
   reads **Connected**. No key is needed.

2. Switch on **Receive models from the LAN drop site**. Share the displayed LAN link or QR code. The sender chooses models and sends directly.

## Run it on another Docker host on the network

The container does not have to run on the computer with Bambu Studio.

1. Copy the `lan-model-drop` folder to the other host.
2. On that host, run `docker compose up -d` in the folder, and open its
   firewall for port 8833 if it has one.
3. In Bambu Studio, set the site address to `http://<the host's address>:8833`.
   Invite links then use that address.

When the page sits behind a reverse proxy, or people should use a fixed name,
set `DROP_PUBLIC_URL` (see [Settings](#settings)); Bambu Studio then uses that
address for invite links and QR codes.

## No station key

The station needs no key. Bambu Studio lists, downloads and removes waiting
models with the site address alone, so anyone who can reach the port can also
read the waiting models and remove them. The person at the Bambu Studio
computer sees every model before it is opened, which is why the drop site does
not ask for a key. Keep the port on a network you trust.

Containers older than this change generated a key in `/data/station-key` and
refused requests without it. After you update the container, that file is no
longer read and can stay in the data volume or be removed. A `DROP_STATION_KEY`
left in an old `.env` file is ignored. An older container that still asks for
a key makes Bambu Studio report **Drop box needs an update**: update the
container with `host.bat --detached`.

## Settings

Copy `.env.example` to `.env` in the `lan-model-drop` folder, change what you
need and run `docker compose up -d` again. Every setting is optional.

| Setting | Default | Meaning |
| --- | --- | --- |
| `DROP_PORT` | 8833 | Port on this computer that people open |
| `DROP_STATION_NAME` | Bambu Studio | Name shown on the page ("Sending to ...") |
| `DROP_MAX_BYTES` | 268435456 (256 MB) | Largest model accepted, at most 2147483648 (2 GB), the largest Bambu Studio takes |
| `DROP_TTL_HOURS` | 24 | Hours a model waits for Bambu Studio before it is removed (at most 720) |
| `DROP_QUEUE_MAX_FILES` | 50 | Most models waiting at once, at most 1000, the most Bambu Studio lists |
| `DROP_QUEUE_MAX_BYTES` | 2147483648 (2 GB) | Most bytes waiting at once |
| `DROP_PUBLIC_URL` | none | Address for invite links behind a reverse proxy, such as `https://drop.example.org` |

An invalid value stops the container with a message in
`docker compose logs` instead of being guessed. `DROP_PUBLIC_URL` must be an
`http://` or `https://` address without a user name, password, query or
fragment. Its host must be a name of letters, digits and hyphens, an IPv4
address or an IPv6 address in brackets, its port 1 to 65535, its path plain
address characters without empty parts, and the whole address at most 1024
characters. That is exactly what Bambu Studio puts into an invite link, so an
address it could not use stops the container instead of being left out of the
invite without a word.

### Change the port

Add `DROP_PORT=9000` (or another free port) to `.env` and run
`docker compose up -d`. Then:

- change the firewall rule to the new port (remove it and add it again with
  `-LocalPort 9000`);
- change the site address in Bambu Studio to `http://localhost:9000`.

Inside the container the service always listens on 8080; only the published
port changes.

## Update

The installed folder belongs to one application version: each update of
Bambu Studio MD3 installs a new `app-<version>` folder and later removes the
old one. A running container does not depend on the folder, so it keeps
working. To move to the new version of the page, run in the new version's
`lan-model-drop` folder:

```powershell
../host.bat --detached
```

The Compose project remains named lan-model-drop and retains its existing data volume. Copy your private .env into the new folder before starting, or use --env-file with its existing path. Never share that file.

## Stop it

| What you want | Command |
| --- | --- |
| Pause it until you start it again | `docker compose stop` |
| Start it again | `docker compose start` |
| Remove the container, keep the waiting models | `docker compose down` |
| Remove everything, including the waiting models | `docker compose down -v` |

Bambu Studio reports **Not reachable** while the container is stopped and
keeps checking at a slower pace.

## Privacy and security

- **What is kept.** Each waiting model is stored with its cleaned file name,
  the optional sender name, its size, its SHA-256 and the time it arrived. It
  is removed when Bambu Studio collects it or after `DROP_TTL_HOURS`.
- **What is logged.** Random item ids, model types and sizes, and removals. Never file names or sender names.
- **Plain HTTP.** Other people on the network can observe traffic. Use only a trusted LAN, or an authenticated HTTPS reverse proxy within that LAN. Never forward the port to the internet.
- **Nothing leaves your network.** The page loads nothing from the internet:
  its fonts and icons are bundled, and it has no analytics. The container
  needs no internet access once the image is built.
- **No station key.** Anyone who can reach the port can list, download and
  remove waiting models, so keep it on a trusted network and never share your
  `.env`.
- **Your wording stays in your browser.** A personal wording file (`.json`,
  the same version 1 file Bambu Studio accepts) can be dropped anywhere on the
  page or chosen in its **Your wording** card. It is checked in full first; a
  file that is not valid changes nothing. The page then shows its own text in
  that wording, keeps the entries in that browser's storage, and never sends
  the file or its entries to the drop box. **Use the original wording** clears
  it. Other people using the page see the original wording.
- **What is accepted.** Only 3MF, STL, STEP, OBJ and AMF, checked by file
  name and by content, up to the size limits. File names are cleaned (control
  characters, characters that reorder or hide text, and `<>:"/\|?*` are
  removed) and only shown; models are stored under random names. A name with
  nothing left after cleaning is refused while the sender is still on the
  page. Bambu Studio checks the size,
  the SHA-256 and the type again after downloading, and opens a model only
  when you choose **Open**.
- **A locked-down container.** The service runs as an unprivileged user on a
  read-only file system with no Linux capabilities, cannot gain privileges,
  has memory, CPU and process limits, and writes only to its data volume. Its
  responses carry a strict Content-Security-Policy and other security headers.

## If something does not work

| What you see | What to check |
| --- | --- |
| The page does not open on the phone | The phone is on the same network, not a guest network; the firewall rule exists; `docker compose ps` shows `healthy` |
| Too many upload attempts | Wait five minutes before retrying |
| "The drop box is full" | Open or discard the waiting models in Bambu Studio |
| Bambu Studio says **Drop box needs an update** | The container is older than Bambu Studio and still asks for a key. Run `host.bat --detached` to update it |
| Bambu Studio says **Not reachable** | Docker Desktop is running and the container is started |

## What has been checked

The service, the page and the container settings are covered by automated
tests in `tests/lan_model_drop/`: every accepted type and refusal, the
station API, the invite link handling in a real headless Chromium, the
container definition and the Windows install rule. The container was built
and run with Docker Engine on Linux, exercised as a sender and as the
station, and reported healthy. Both halves were also run together:
`tests/lan_model_drop/container_capture.py` builds the container with Docker
Compose, sends every sample model through the real page in headless Chromium
and again with curl using the page's own headers, drives the station API the
way Bambu Studio does, and keeps every answer in
`tests/lan_model_drop/container_fixtures/`. `container_fixtures.test.mjs` reads
those answers with Bambu Studio's own station code and with the page's own
answer handling, so a change to one half that the other would not understand
fails the tests. Running it under Docker Desktop on Windows,
the firewall rule, and sending from a real phone on a real network still need
to be observed on a Windows computer.
