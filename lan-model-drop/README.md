# LAN model drop

## Direct sending update (1.0.1)

Mixed-version compatibility: protocol-1 status includes directSend:true and an inert dropCode value of not-required for older receivers. The authenticated legacy renewal endpoint returns 410 retired, never a successful rotation or revocation. An older desktop may briefly show a connection error after trying renewal; the next status poll restores collection. The inert status value is never an upload credential. New receivers keep collecting from older services but hide direct invite links until the service advertises the capability, and show an upgrade instruction. Older desktop versions retain their old code labels until updated.

Invite links contain only the address, with no credentials or query parameters. Code-renewal controls are removed. Legacy DROP_CODE configuration and the existing drop-code file are ignored without deleting either. Upgrades retain the same volume, station key and waiting models. Model-format, size, queue and expiry limits remain enforced.

Anyone who can reach the service can send; inbox access, downloads and management still require the station key. Each source address allows 60 upload attempts per minute, followed by a five-minute cooldown. Use a trusted local network and never expose the service to the internet. Container 1.0.1 was published successfully by [run 42418444721](https://github.com/Ding-Ding-Projects/BambuStudio/actions/runs/42418444721) from source `601003f9b97d8945732d5a166f8dbe2b061d040f`. The verified image digest is `sha256:9382599181a32a5d78730c82b315612fcadc51beacea0ad3b831500e122ce51a`; build-context SHA-256 is `4fb31a53d02699892ba4669a012900b13b7549040bba6e9d1f8cbf3a4424a389`. The exact digest was pulled and an isolated Compose project passed `up --wait`, local health and second-host LAN health checks. Sender GUI verification remains pending. The existing user service remains on 1.0.0 until that verification completes; isolated verification is not an applied service update.

A small web page on your local network where other people (phones, tablets,
laptops) send 3D models to Bambu Studio MD3 on this computer. It runs in a
Docker container; Bambu Studio collects what arrives and asks you whether to
open or discard each model. Nothing is opened, sliced or printed by itself.

Full guide: `docs/features/application-integration/lan-model-drop-site.md`
in the Bambu Studio MD3 source, also in the application's Help.

## Start it

With Docker Desktop running, open a terminal in this folder and run:

```powershell
..\host.bat --detached
```

The launcher pulls the published versioned image, preserves the data volume,
waits for health, and prints the complete LAN address. The default port is 8833.
Check that address from the sending device too. Existing network protections are
not changed. To view live service status and logs, run:

```powershell
..\host.bat --logs
```

## Connect Bambu Studio

In Bambu Studio, open Preferences, LAN model drop, and choose **Start local drop
server**. The native action transfers the station key through a protected local
pipe and saves the connection. It never prints the key. A failed setup leaves a
visible result and preserves existing settings.

## Settings

Copy .env.example to .env, adjust the port, file-size, queue, retention and LAN-address settings, then run docker compose up -d.

## Stop, update, remove

| Task | Command |
| --- | --- |
| Stop (models and key are kept) | `docker compose down` |
| Update after installing a new Bambu Studio MD3 | `..\host.bat --detached` |
| Remove everything, including waiting models and the key | `docker compose down -v` |

## What it does and does not do

- Accepts 3MF, STL, STEP, OBJ and AMF only, checked by name and by content.
- Keeps models only until Bambu Studio collects them, or 24 hours.
- Loads nothing from the internet, has no analytics, and logs no file names,
  codes or keys.

After pairing, turn on receiving and share the invite link or QR code. Keep .env private because it may contain the station key.
