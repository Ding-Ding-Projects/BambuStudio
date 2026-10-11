# LAN model drop site

The LAN model drop site is a small web page that runs in a Docker container
on your local network. Other people open it on a phone, tablet or laptop and
send 3D models to Bambu Studio MD3 on your computer. Bambu Studio collects
each model, shows who sent it and asks you to open or discard it. Nothing is
opened, sliced or printed by itself.

This article covers the container: starting it, reaching it from other
devices, the station key, settings, updates and security. In Bambu Studio the
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
   docker compose up -d
   ```

   The first start builds the image, which downloads the Node.js base image
   once. Later starts take a few seconds.
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

1. Print the station key in the `lan-model-drop` folder:

   ```powershell
   docker compose exec lan-model-drop node server/station-key.mjs
   ```

2. In Bambu Studio open **Preferences > LAN model drop**, paste the key into
   **Station key**, keep the site address `http://localhost:8833` and choose
   **Test connection**. The status reads **Connected**.
3. Switch on **Receive models from the LAN drop site**. Bambu Studio then
   shows the invite: a link such as `http://<this computer's address>:8833/#code=482915`
   and a QR code of the same link. Send the link or let the other person scan
   the code.

The page fills in the drop code from the link, removes it from the address
bar, and the person only chooses models and sends them. **New link** in Bambu
Studio makes a new drop code, so older links stop working.

## Run it on another Docker host on the network

The container does not have to run on the computer with Bambu Studio.

1. Copy the `lan-model-drop` folder to the other host.
2. On that host, run `docker compose up -d` in the folder, and open its
   firewall for port 8833 if it has one.
3. Read the station key there with
   `docker compose exec lan-model-drop node server/station-key.mjs`.
4. In Bambu Studio, set the site address to `http://<the host's address>:8833`
   and paste the key. Invite links then use that address.

When the page sits behind a reverse proxy, or people should use a fixed name,
set `DROP_PUBLIC_URL` (see [Settings](#settings)); Bambu Studio then uses that
address for invite links and QR codes.

## Read or replace the station key

The station key lets Bambu Studio list, download and remove the models that
arrive. The container makes one on its first start, 32 random bytes written
as 43 characters, and keeps it in its data volume. Print it at any time:

```powershell
docker compose exec lan-model-drop node server/station-key.mjs
```

The key is never written to the container log. To replace it, for example
after it was shown to someone, remove the stored key and restart:

```powershell
docker compose exec lan-model-drop rm /data/station-key
docker compose restart
docker compose exec lan-model-drop node server/station-key.mjs
```

Then paste the new key into Bambu Studio. You can also choose the key
yourself with `DROP_STATION_KEY` in a `.env` file.

## Settings

Copy `.env.example` to `.env` in the `lan-model-drop` folder, change what you
need and run `docker compose up -d` again. Every setting is optional.

| Setting | Default | Meaning |
| --- | --- | --- |
| `DROP_PORT` | 8833 | Port on this computer that people open |
| `DROP_STATION_NAME` | Bambu Studio | Name shown on the page ("Sending to ...") |
| `DROP_MAX_BYTES` | 268435456 (256 MB) | Largest model accepted |
| `DROP_TTL_HOURS` | 24 | Hours a model waits for Bambu Studio before it is removed (at most 720) |
| `DROP_QUEUE_MAX_FILES` | 50 | Most models waiting at once |
| `DROP_QUEUE_MAX_BYTES` | 2147483648 (2 GB) | Most bytes waiting at once |
| `DROP_STATION_KEY` | generated | Station key, 16 to 512 visible characters without spaces |
| `DROP_CODE` | random, 6 digits | A fixed drop code of 4 to 12 digits; it cannot be renewed from Bambu Studio |
| `DROP_PUBLIC_URL` | none | Address for invite links behind a reverse proxy, such as `https://drop.example.org` |

An invalid value stops the container with a message in
`docker compose logs` instead of being guessed. `DROP_PUBLIC_URL` must be an
`http://` or `https://` address without a user name, password, query or
fragment.

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
docker compose up -d --build
```

The compose project is always named `lan-model-drop`, so this replaces the old
container and keeps the same data volume: the station key, the drop code and
any waiting models stay. If you use a `.env` file, copy it into the new
folder first, or keep it somewhere permanent and add
`--env-file <path to your .env>` to each `docker compose` command.

## Stop it

| What you want | Command |
| --- | --- |
| Pause it until you start it again | `docker compose stop` |
| Start it again | `docker compose start` |
| Remove the container, keep the key and waiting models | `docker compose down` |
| Remove everything, including the key and waiting models | `docker compose down -v` |

Bambu Studio reports **Not reachable** while the container is stopped and
keeps checking at a slower pace.

## Privacy and security

- **What is kept.** Each waiting model is stored with its cleaned file name,
  the optional sender name, its size, its SHA-256 and the time it arrived. It
  is removed when Bambu Studio collects it or after `DROP_TTL_HOURS`. The
  station key and the drop code are files readable only by the service.
- **What is logged.** Random item ids, model types and sizes, lockouts and
  removals. Never file names, sender names, drop codes or the station key.
- **Nothing leaves your network.** The page loads nothing from the internet:
  its fonts and icons are bundled, and it has no analytics. The container
  needs no internet access once the image is built.
- **The drop code.** Senders need the current drop code. Five wrong codes
  within a minute lock that device out for five minutes. In an invite link
  the code travels after `#`, which browsers never send to a server or pass
  on to other sites, and the page removes it from the address bar as soon as
  it opens. Anyone who has a current link can send models until you choose
  **New link**.
- **The station key.** Only Bambu Studio needs it. Anyone with the key can
  list, download and remove waiting models, so never put it in an invite or
  share your `.env`. Bambu Studio stores it protected for your Windows user
  account.
- **What is accepted.** Only 3MF, STL, STEP, OBJ and AMF, checked by file
  name and by content, up to the size limits. File names are cleaned and only
  shown; models are stored under random names. Bambu Studio checks the size,
  the SHA-256 and the type again after downloading, and opens a model only
  when you choose **Open**.
- **Plain HTTP.** The page uses HTTP on your local network. People on the same
  network can see what is sent, and anyone there who knows the drop code can
  send a model. Use it on networks you trust. For anything else, put it behind
  an HTTPS reverse proxy and set `DROP_PUBLIC_URL`. Never forward the port to
  the internet from your router.
- **A locked-down container.** The service runs as an unprivileged user on a
  read-only file system with no Linux capabilities, cannot gain privileges,
  has memory, CPU and process limits, and writes only to its data volume. Its
  responses carry a strict Content-Security-Policy and other security headers.

## If something does not work

| What you see | What to check |
| --- | --- |
| The page does not open on the phone | The phone is on the same network, not a guest network; the firewall rule exists; `docker compose ps` shows `healthy` |
| "The drop code was not accepted" | Ask for a new link; the code may have been renewed |
| "Too many wrong drop codes" | Wait five minutes, then use the link again |
| "The drop box is full" | Open or discard the waiting models in Bambu Studio |
| Bambu Studio says **Wrong station key** | Print the key again and paste it |
| Bambu Studio says **Not reachable** | Docker Desktop is running and the container is started |

## What has been checked

The service, the page and the container settings are covered by automated
tests in `tests/lan_model_drop/`: every accepted type and refusal, the
station API, the invite link handling in a real headless Chromium, the
container definition and the Windows install rule. The container was built
and run with Docker Engine on Linux, exercised as a sender and as the
station, and reported healthy. Running it under Docker Desktop on Windows,
the firewall rule, and sending from a real phone on a real network still need
to be observed on a Windows computer.
