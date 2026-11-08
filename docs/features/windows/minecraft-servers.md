# Minecraft servers

**Surface:** File > Minecraft servers, the command palette (`Ctrl+Shift+F`, type
"Minecraft") and the Every feature page →
`src/slic3r/GUI/GameServers/ServersPanel.{hpp,cpp}`. The rules (settings,
versions, console lines, backups, the world generator) live in
`ServerModel.{hpp,cpp}`, and what touches this PC (Java, ports, files, zip
files, downloads, the server process) in `ServerSystem.{hpp,cpp}`, all in the
same folder.

Java servers on this PC that you create here or adopt from a folder, each with
its console, every setting, players, worlds, versions, backups, schedules, a
world generator and history. Running a server needs Windows in this build;
everything that only reads files works everywhere.

## The list

**Create server...** sets up a new Vanilla server, **Find and adopt...** adds
servers that already exist, and **Remove from list...** forgets one; its folder,
worlds and backups stay where they are. Each row shows the state (Stopped,
Starting, Running with how many are online, Stopping) and the version. The line
under the title lists the Java installations found and this PC's memory.

Java is found in `JAVA_HOME`, `PATH`, the usual install folders (Oracle,
Eclipse Adoptium, Microsoft, Zulu, BellSoft, Amazon Corretto and others) and the
Minecraft Launcher's own runtimes, and each version is read from the
installation's `release` file without running anything. **Find Java again** on
the Overview reads them again after you install one.

## Create

Read the Mojang version list first (Versions, **Read the Mojang list**), then
choose a release, a name, the folder it goes in (it must be new or empty), the
most memory and the port (the first free one from 25565). The server needs the
Minecraft EULA accepted: the dialog's check box does that, with a link to read
it, and nothing is created without it.

The official server file is downloaded and checked against the SHA-1 in
Mojang's record for that version; a file that does not match is not kept. Then
`eula.txt` is written (accepted, with the date) and `server.properties` with the
port and the name as the message of the day. The server writes the rest of its
settings the first time it starts.

## Find and adopt

Choose a folder: it is searched three levels deep for folders with a server jar
and a `server.properties` or `eula.txt`. Each one found is listed with its jar,
flavour and version; adopting adds it to the list and changes nothing in its
folder. Vanilla, Paper, Purpur, Spigot, Fabric, Forge, NeoForge and Velocity are
recognised from the jar's name and the `version.json` and manifest inside it.

## Starting and stopping

**Start** checks everything first, and each check that fails says what to do:

- Java new enough for the version: 8 before 1.17, 16 for 1.17, 17 up to 1.20.4
  and 21 from 1.20.5, or what the jar's own `version.json` says. Java is picked
  automatically as the closest version that is new enough.
- The server file is there and `eula.txt` has the EULA accepted.
- The port is free, and the most memory leaves at least 1 GB for the rest of
  this PC.
- No other program has the world open (its `session.lock`).

The server runs as `java -Xms -Xmx`, with the console in UTF-8 and log4j's
lookups turned off (`-Dlog4j2.formatMsgNoLookups=true`, which protects older
versions), your own JVM arguments, `-jar` and `nogui`. It has no window of its
own, and only its console pipes are handed to it.

**Stop safely** sends `stop`, which saves the world. **Restart** stops it and
starts it again. **Force stop...** sits behind the two-key confirmation and ends
the process at once, without saving.

Servers keep running while this page is closed. When the app exits, every
running server is asked to stop and given up to 20 seconds to save.

## Overview

Cards for the state, who is online, memory (what the process holds against the
most it may use), how long it has been up, the version and flavour, the Java it
runs with, and the next backup and restart. **Recent events** lists starts,
joins, leaves, advancements, backups and errors. **Open the full log** opens
`logs/latest.log`.

## Console

Everything the server prints (the last 3,000 lines), and a command line. Up and
Down bring back earlier commands; **list** and **save-all** are one click.
Commands run as the server console, with every permission.

## Properties

Every setting in `server.properties` this app knows (over 60) has its own
control: a switch, a choice, a number with its range, text, or a hidden
password. They are grouped (Gameplay, World, Players, Network, Performance,
Resource packs, Advanced), the search finds a label, key or explanation, and
hovering a row shows what it does, its default and the version that brought it.
A setting the server's version does not have is not offered:
`simulation-distance` before 1.18, for example, or `spawn-animals` and
`spawn-npcs` from 1.21.2.

**Save changes** checks every value against its range first. The file is
rewritten so that every line you did not change stays exactly as it was:
comments, settings this app does not know, escapes and line breaks included.
Changed values are written the way Java writes properties (`\:` and `\=`, and
`\uXXXX` for anything outside ASCII, which every version reads).

Settings marked **Live** change at once while the server runs: difficulty
(`difficulty`), the game mode new players get (`defaultgamemode`) and the
allowlist switch (`whitelist on` or `off`). The others take effect at the next
start, and the note after saving says how many. **Undo last save** puts the
file back as it was before the last save in this session.

## Players

The allowlist, the operators (levels 1 to 4, each with what it allows) and the
bans. While the server runs, **Add...** and **Remove** go through its own
commands (`whitelist add`, `op`, `ban`, and their opposites), so it looks each
player up and writes its own lists. While it is stopped, **Remove** edits the
list file, and an operator's level can be changed (the server keeps the list in
memory while it runs). A list file that is not valid JSON is shown as found and
never rewritten. **Online now** comes from the console: joins, leaves and the
answer to `list`.

## Worlds

Every folder in the server folder with a `level.dat`. **Run this world** sets
`level-name`, which takes effect at the next start.

## Versions

**Read the Mojang list** reads every version Mojang publishes (kept for next
time) and sorts it into releases, snapshots, and old alpha and beta, each with
the Java it needs and whether this PC has it. **Switch to this version...**
works on a stopped Vanilla server: a full backup is made first, then
`minecraft_server.<version>.jar` is downloaded and checked by SHA-1, and the
server uses it from the next start. The old server file stays in the folder.
Going back to an older version asks first: worlds saved by a newer version may
not load, and the backup is the way back.

## Backups

**Back up now** makes a zip of the worlds (`level-name` and its `_nether` and
`_the_end` folders) and the settings and player files. While the server runs,
saving is paused for the copy: `save-off`, `save-all flush`, the zip once the
server says the game is saved, then `save-on`. `session.lock` is left out. The
zip is written under a temporary name, renamed when complete, and every file in
it is read back and checked against its CRC before the backup counts.

Backups are named `<server> <date> <time>.zip`, with `auto` at the end for
automatic ones, and go to the server's `backups` folder, or to a folder of your
own with a folder for each server (**Keep backups in...**).

**Restore...** works on a stopped server, behind the two-key confirmation. The
backup is extracted into a temporary folder first; then each world and file in
it replaces the one in the server folder, and the current one is kept beside it,
renamed `before restore` with the time. Nothing is deleted. **Delete...** also
asks for both keys.

## Schedule

Automatic backups every 1 to 168 hours, keeping the newest 1 to 100 automatic
ones (older automatic backups are deleted; backups made by hand never are), and
a daily restart at a time you choose, with a chat warning 0 to 30 minutes
before. Schedules run while Bambu Studio is open. The first automatic backup
comes one period after the schedule is saved, and one that fails is tried again
after 10 minutes.

## World generator

Makes a new world with the selected server's own server file and version,
without touching that server's world. The run happens in a folder of its own
under the data folder, with a server that listens on this PC only
(`127.0.0.1`, a free port from 25600), lets nobody in (an empty allowlist)
and has the EULA accepted for that run only by the check box.

- **Seed** (the dice picks a random 64-bit one), **world type** (Normal, Flat,
  Large biomes, Amplified, Single biome; each version's own `level-type` value),
  **mode**, **difficulty**, **hardcore** (which keeps the difficulty hard) and
  **structures**.
- **Flat:** the layers, top first (add, reorder, remove, or start from the
  classic flat world), the biome, lakes, features and which of the 20 structure
  sets to place, written as the `generator-settings` JSON of 1.19 and newer,
  bottom layer first as the server reads it. Before 1.19 only the classic flat
  world can be made.
- **Single biome:** the biome (1.19 and newer).
- **Game rules:** every rule the version has, typed; only the ones you change
  are applied, with `gamerule`, once the server has started.
- **Area:** 0 to 1,000 chunks each way, centred on a point you choose. It is
  made in batches of 16 x 16 chunks, the most one `forceload` takes: each batch
  is force loaded, `execute if loaded` is asked at its corners and middle until
  every answer passes, and the batch is released. Making an area needs 1.19.4 or
  newer; older versions make the spawn area only. A batch that takes more than
  four minutes stops the run. The estimate counts about 40 chunks a second and
  12 KB a chunk.

When every batch is done, the world is saved (`save-all flush`), the server
stops, and the world is copied to a new folder, or zipped under a temporary
name and checked; nothing is ever overwritten. The run's folder is removed
afterwards. **Cancel** stops the run and writes nothing. The preview draws the
area with thin lines every 4 chunks and thick lines at the batches, and fills
each batch as it is done.

## History

Every start, stop, settings save, player change, backup, restore, version
switch and world made, for each server. The last 300 are kept.

## Stored files

| File | Holds |
| --- | --- |
| `minecraft-servers.json` in the data folder | The servers, their schedules, the history and the backup folder |
| `minecraft-version-list.json` in the data folder | The Mojang version list as last read |
| `minecraft-world-runs\` in the data folder | A world generator run's folder while it runs |

## Checks

`tests/game_servers.test.mjs` compiles `tests/game_servers_test.cpp`: version
ids and their order (data versions, and snapshots by date), the Java each
version needs, `server.properties` read and written back byte for byte (Windows
line breaks, comments, continued lines, escapes, a key given twice), the
settings table (no key twice, every default valid, ranges, live commands and
versions), the EULA, player lists, server jars of eight flavours, console lines,
the Mojang list and version records, the start checks and command line, backup
names and pruning, schedules, the world generator's settings, problems and
properties, its batches (100 x 100 chunks make 49 batches of at most 256 chunks
that cover the area exactly once), the library file and SHA-1. Its negative
build rewrites every line of `server.properties` and plans batches of 17 x 17
chunks, and must fail.

## Not done yet

Docker, servers over SSH, Bedrock servers, installing flavours other than
Vanilla (they are recognised when adopted), the plugin and pack catalogue,
checking commands against the version's command tree, the memory chart, ticks
per second, keeping the remote console password in the Windows vault, dragging
the generator's area on the preview, and the generator's sea level, lowest Y and
height (they need a data pack) are designed but not built.
