# LAN model drop

A small web page on your local network where other people (phones, tablets,
laptops) send 3D models to Bambu Studio MD3 on this computer. It runs in a
Docker container; Bambu Studio collects what arrives and asks you whether to
open or discard each model. Nothing is opened, sliced or printed by itself.

Full guide: `docs/features/application-integration/lan-model-drop-site.md`
in the Bambu Studio MD3 source, also in the application's Help.

## Start it

With Docker Desktop running, open a terminal in this folder and run:

```powershell
docker compose up -d
```

The page listens on port 8833 of every network interface of this computer.
Allow the port through Windows Firewall for private networks once, from an
administrator PowerShell:

```powershell
New-NetFirewallRule -DisplayName "Bambu Studio LAN model drop" -Direction Inbound -Protocol TCP -LocalPort 8833 -Action Allow -Profile Private
```

## Connect Bambu Studio

Print the station key and paste it into Bambu Studio, Preferences, LAN model
drop:

```powershell
docker compose exec lan-model-drop node server/station-key.mjs
```

Then switch on "Receive models from the LAN drop site". Bambu Studio shows an
invite link and a QR code to hand to the other person; the link carries the
drop code, so they only choose their models and send.

## Settings

Copy `.env.example` to `.env` and change what you need (port, station name,
size limits, how long models wait, a fixed drop code or a public address).
Then run `docker compose up -d` again. Keep `.env` private: it can hold the
station key.

## Stop, update, remove

| Task | Command |
| --- | --- |
| Stop (models and key are kept) | `docker compose down` |
| Update after installing a new Bambu Studio MD3 | `docker compose up -d --build` |
| Remove everything, including waiting models and the key | `docker compose down -v` |

## What it does and does not do

- Accepts 3MF, STL, STEP, OBJ and AMF only, checked by name and by content.
- Asks for the drop code shown in Bambu Studio; five wrong codes in a minute
  lock that device out for five minutes.
- Keeps models only until Bambu Studio collects them, or 24 hours.
- Loads nothing from the internet, has no analytics, and logs no file names,
  codes or keys.
- Uses plain HTTP on your local network. Anyone on the same network who has
  the drop code can send a model, and the network can see what is sent. Use it
  on networks you trust, or put it behind an HTTPS reverse proxy and set
  `DROP_PUBLIC_URL`.
