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

## Verification and remaining scope

All eight edition/target pairs have partial implementation. **Zero of eight
have complete playable acceptance evidence.** “Tested” below identifies the
specific test performed. The tests do not establish that an authenticated
player can join a running instance.

| Target and edition | Create, start, stop and console | Settings and runtime metrics | Content and world creation | Backup, restore and reconnect evidence |
| --- | --- | --- | --- | --- |
| Local process, Java | Mojang manifest and trusted runnable JAR setup; existing-folder adoption; managed Java process and console | Typed local properties; process memory history; supported Paper TPS; process-bound RCON client credential | Local JAR/ZIP and version-filtered Modrinth install; native generator with folder/ZIP output and exact-JAR terrain packs | Existing checked ZIP backup/restore. Official 1.21.4 metadata and pack writing tested. Server startup, generated-world reopen and player reconnect await authorized runtime acceptance. |
| Local process, Bedrock | Official Windows stable/preview discovery and ZIP setup; controlled process, console and stop | Typed properties from installed server; confidential values protected. No native time-series metrics | Resource/behavior pack installation and explicit world registration; native world creation uses the dedicated server's own settings | Live local world backup/restore and a compatible Bedrock client session are unverified. |
| Local Docker, Java | Version and eight loader selections; create/start/stop by owned ID; RCON adapter | Per-target property editor and Docker CPU, memory and PIDs | Distribution setup and server-side world creation. Bespoke terrain authoring is local-Java only | World snapshots and isolated recovery volume. Local create/start/stop tested with EULA false; no playable process asserted. |
| Local Docker, Bedrock | Version selection; create/start/stop and Bedrock console adapter; separate NetherNet or RakNet mappings | Per-target property editor and Docker metrics | Image setup and first-start world creation; no Java terrain-pack generator | Local fixture verified one TCP and sixteen UDP bindings. Real helper tested synthetic Bedrock restore; no authenticated client join. |
| SSH process, Java | Upload-independent provisioning from trusted runnable JAR already on target; managed owner marker, lock, console and graceful stop | Target file owner, revision and public property allowlist enforced; no remote time-series metrics | Native Java creation from selected distribution. Local-Java advanced world generator has no direct SSH execution | Official Java JAR copied and byte-compared over real SSH, EULA remained false. Synthetic world restore tested separately. Actual Java reconnect pending. |
| SSH process, Bedrock | Provision trusted Linux ZIP on host; owner-bound native process and console | Target settings with the actual file owner and revision; no remote time-series metrics | Dedicated server creates worlds from its configured settings | Inert process fixture verified owner check, list and stop. Synthetic Bedrock backup/restore verified. Real distribution, pack enablement and client reconnect pending. |
| SSH Docker, Java | Same owned-ID lifecycle and RCON adapter through a configured Docker SSH context | Target settings and CPU/memory/PID readout | Same container distribution setup as local Docker | Real SSH context exercised create/start/stop on a disposable container with EULA false. Network-disabled helper restored a synthetic world on the target. |
| SSH Docker, Bedrock | Same edition-specific lifecycle and transport through selected SSH context | Target settings and Docker metrics | Same dedicated-server setup as local Docker | Bedrock helper snapshot/restore tested on the Linux target. No real SSH-context Bedrock service or playable session verified. |

### Original gaps and residual work

- **Distributions and versions.** Java creation lists every record returned by
  Mojang and checks whether the selected version has a server download.
  Microsoft's Bedrock index reports current Windows/Linux stable and preview
  records. Its missing historical releases and missing publisher checksums
  remain explicit. A local JAR can be installed without running a third-party
  installer.
- **Plugins and packs.** The local Java installer handles trusted plugins,
  loader-specific mods and world data packs. The catalog filters by server
  version and loader. Bedrock validates package UUIDs, versions and modules.
  Complete dependency resolution, in-place update/removal and acceptance
  against every real loader are incomplete.
- **Commands and observability.** The selected Java JAR generates a
  hash-bound Brigadier report. Supported primitive arguments and literal
  paths are checked before sending. Selectors, registry arguments, SNBT
  and plugin-added grammar still require the server parser. Paper-compatible
  one-minute TPS and local working-set history have implemented paths;
  unsupported variants do not display fabricated measurements.
- **World authoring.** The Java generator supports direct area selection,
  keyboard coordinates, bounded batches and source-derived normal terrain
  settings. The official 1.21.4 data generator returned 84 command roots
  and 1,095 blocks; its nested JAR supplied 65 biomes, 20 structure sets
  and pack format 61. Additional distribution schemas, complete registry
  coverage and playable same-version reopen remain unverified.
- **Credentials and recovery.** The local Windows client vault is tested
  with a disposable RCON endpoint, including a wrong-process refusal. The
  server still uses its own configured credential. SSH and container
  settings return only supported nonconfidential fields. Isolated backups
  and restores have verified fixtures; a fixture is not production-world
  recovery or an authenticated player login.

## Container servers

Open **File > Minecraft servers > More targets > Container servers**. Enter an existing Docker
context, either a local engine or an SSH context configured in the Docker CLI.
Choose Java or Bedrock, the server version, the container image tag, the host
port and, for Java, the distribution. Java offers Vanilla, Paper, Purpur,
Spigot, Fabric, Forge, NeoForge and Quilt. Version and processor support
depend on the selected image and distribution.

Create requires acceptance of the Minecraft EULA. It creates a stopped
container with a fresh persistent data volume. Java maps TCP 25565. Current Bedrock NetherNet maps TCP 19132 for signaling
and sixteen UDP ports for gameplay. The default UDP range is 19140 through
19155 and the first host port is configurable. Legacy RakNet maps UDP 19132. Both default to host loopback. For LAN play, enter the target
host's private IPv4 address and explicitly enable LAN connections. Start the
selected container, then refresh to read Docker's reported state. Stopping a
container preserves its world. A running container may still be downloading
or starting its game server.

Only containers whose immutable IDs this installation recorded for the
selected context appear in the list. A matching container label on its own
does not confer control. Docker handles the SSH connection through the named
context; the dialog takes no SSH password or private key.

| Manager target | Java | Bedrock |
| --- | --- | --- |
| Local process | Vanilla creation and existing Java server management | Trusted Windows ZIP installation, launch, console, stop |
| Local container | Creation, listing, start and stop | Creation, listing, start and stop |
| SSH process | Provision trusted JAR in a new directory, start, console, stop | Provision trusted Linux ZIP in a new directory, start, console, stop |
| SSH container via Docker context | Creation, listing, start and stop implemented; end-to-end verification pending | Creation, listing, start and stop implemented; end-to-end verification pending |

This target inventory covers eight edition and target combinations. All eight have
partial workflows. Full version, operational,
content installation, metrics and restore coverage is still pending.

### Native process targets

Open **More targets > Native console and Bedrock setup**. To provision a
local Bedrock server, choose **Install Bedrock ZIP**, select a trusted Windows
dedicated-server distribution, then its parent directory. The manager
extracts into a new directory named after the ZIP and refuses an existing
destination. Review its server.properties and network settings before start.

For a new SSH installation, enter a trusted existing OpenSSH identity, the
absolute package path on that host and a new absolute destination directory.
**Provision new SSH server** checks the package and available filesystem,
copies the Java server JAR or extracts the Bedrock Linux distribution, sets
the port and records ownership. It refuses an existing destination. The Java
setup binds to loopback and leaves the EULA unaccepted. Starting from the
dialog after explicitly accepting the EULA activates that newly provisioned
Java server. An owned remote launch verifies the installation marker and takes
a per-instance lock. No SSH password or private key enters the manager.

For an existing local Bedrock installation choose its directory. For an SSH
process choose Java or Bedrock, an existing OpenSSH profile and an absolute
Linux server directory. The host must already be trusted and its identity
must work without prompting. Java expects java on PATH, server.jar,
server.properties and the server's own EULA configuration. Bedrock expects
an executable bedrock_server and server.properties. Review network settings
and accept the EULA before starting. The dialog streams output, supports
single-line console commands and can send a graceful stop. A successful SSH
process launch alone does not indicate that the game server has become ready.
### Install Java plugins, mods and data packs

On a stopped local Java server, choose **Overview > Install plugin, mod or
data pack** and select a trusted local package for the server's version.
The manager checks the loader descriptor, places Bukkit/Paper plugins under
`plugins`, or Fabric, Forge, NeoForge and Quilt mods under `mods`. A ZIP with
a valid `pack.mcmeta` and `data/` tree goes in the configured world's
`datapacks`. Publication checks the copied bytes and refuses an existing
filename. The source is retained. Start the server and check its log for
game-version compatibility, dependency errors and successful loading.

**Browse catalog** searches the public Modrinth catalog by text, server version
and loader, with 30 results per page. In regular-expression mode the pattern
filters the current page locally. Select a project, show compatible packages,
then choose the exact file to install. Only HTTPS downloads from the catalog's
content host are accepted; the published SHA-1 and byte count must match
before installation. An occupied filename is preserved.

In the local **Native console and Bedrock setup** dialog, **Install Bedrock
pack** accepts a trusted .mcpack or ZIP with a validated root manifest. It
expands into the server's behavior_packs or resource_packs directory under its
UUID. To activate it, stop the world, choose **Activate installed pack** and
select that pack's manifest. The manager checks the database lock, preserves
existing registrations and adds the selected ID and version to the configured
world's pack list. The next world start loads the pack. Mixed multi-pack
archives and automatic dependency installation are not supported.

The local Java overview charts the process working set from real samples.
While the overview is visible it samples at most every five seconds; the
background timer samples every thirty seconds. The chart retains thirty
minutes for the current run. **Read Paper tick rate** asks a running Paper or
Purpur console for its one-minute TPS figure. Unrecognized responses and
stale readings are shown as unavailable. The value is measured by the
server, independently of the configured tick-rate target.

In **Properties > RCON credential**, save or clear the client credential for
the selected local Java server in Windows Credential Manager. The control
never displays the stored value. **Test loopback login** checks that the
connected peer is the selected running server process before authenticating,
then requests a player list without displaying the response. Saving a client
credential does not enable RCON or change the password that the server itself
reads from server.properties.

The generator starts at 100 × 100 chunks. Drag across the preview to select a
smaller rectangle from the displayed area. The width, depth and center fields
mirror the selected chunk coordinates and remain available for keyboard use.
Setting the dimensions to zero generates the spawn area only.

**Console > Build version report** runs the selected JAR's built-in data
generator with a three-minute limit in an isolated directory. The cache key
is the JAR's SHA-1. **Check command** uses its Brigadier command tree to check
literal paths, redirects, booleans, bounded numeric arguments, strings and
greedy text. Unsupported argument parsers and plugin-only commands are
identified; the running server remains their authority. Invalid arguments
from a supported parser are stopped before sending. A generated block
registry supplies the generator's block picker.

**Read generation registries from server JAR** reads the installed distribution
without executing it. Supported vanilla JARs and version bundles supply their
own biomes, structure sets, normal preset, noise settings, dimension type and
data-pack format. **Customize the normal overworld** controls sea level, lowest
Y, height, aquifers and ore veins. The generated world includes its own data
pack. Its overworld references the customized settings, while original noise
graphs and the other dimensions are preserved. Unsupported distribution
formats report why exact data could not be read.

Container actions include bounded single-line console commands and Docker CPU, memory and process counts. Java uses the image’s RCON client. Bedrock uses its command helper, which writes the response to the server log.

**Native console and Bedrock setup > Bedrock settings** reads the selected
installation's actual property keys. Search supports text and a local regular
expression. Known fields expose the applicable switch, choice, integer or
text control. Applying one field preserves unrelated lines and comments, and
requires reloading if another process edited the file. Unrecognized keys,
duplicates and protected values are not editable.

**Create server** can import a trusted runnable server JAR instead of
downloading Vanilla. Select the distribution, accept the EULA for the new
server and choose a new or empty folder. Setup checks the runnable manifest
and readback, preserves the distribution filename, and publishes its settings
and server file without replacing existing files. Mojang creation lists all
records from its manifest; some historical releases have no server download.

**Native console > Official Bedrock versions** reads Microsoft's live index
for Windows and Linux stable and preview distributions. Download a ZIP or
install the selected Windows build after accepting the EULA. The UI validates
the package contents, computes a SHA-256 and accepts an optional independently
trusted expected hash. The index itself does not provide a historical catalog
or publisher checksums.

### Restore an SSH or container world separately

For a newly provisioned native SSH server open **SSH world recovery**. For a
container recorded by this installation, select it and open **World
recovery**. Stop the owned server before **Create backup**. The snapshot
captures world files, Bedrock packs and selected configuration/player lists.
It excludes transient locks and strips the RCON password and Bedrock debugger
passcode from the property file. Backups are private to the target; protect
them as player data.

Archive creation checks source stability and verifies ZIP CRCs. The receipt
contains the archive name, file count and SHA-256. **Refresh backups** lists
available snapshots. **Restore separately** requires a new directory and
verifies every restored file against the manifest. An SSH restore names its
new absolute path. A container restore names a new directory in the reported
dedicated recovery volume. Neither route writes into the live world.
Container recovery runs a network-disabled Python 3.13 Alpine helper with
read-only server mounts and requires a usable image on the target engine.

**Server settings** also works for Java and Bedrock in an owned native SSH
directory or recorded container. It reads only supported nonconfidential keys
and sends an edit on standard input. Remote writes require an idle owned
instance and a matching file revision. The helper preserves mode, group,
comments, line endings and unedited values. Container configuration writes
use the file owner's numeric identity and a network-disabled helper.

**Install SSH content** and the container **Install target content** action
transfer a selected local JAR, ZIP or .mcpack to an owned stopped server.
Select the installed Java distribution or, for Bedrock, whether to activate
the pack. The target verifies the complete streamed SHA-256 before reading
the package, checks its descriptor, refuses incompatible content and preserves
unrelated files. The Linux container helper has no network access, uses the
server file owner's identity and mounts only the selected container's volumes.
The receipt reports actual installation and any outstanding activation.

**Overview > Manage installed content** lists the selected local Java server's
plugins, mods and data packs, including older installations. Choose the exact
file to disable, enable, update or remove. A disabled JAR/ZIP leaves the
loader's active extension; a removed package is moved out of the loading
directory and can be restored. Updating verifies the replacement's loader
descriptor and keeps the previous archive. **Roll back version** restores it.
The operation rechecks the selected file before publication.

Local Bedrock installations also expose **Back up local Bedrock worlds** and
**Restore local Bedrock backup separately**. The native backup checks the
world database locks and archives generated worlds, packs and selected
settings. It excludes debugger passcodes, RCON passwords and transient
database locks. A server-bound marker must match before restore. The new
restore reads every file back against the ZIP's byte count and CRC. Existing
archives and destination directories are preserved.

Bedrock version refresh retains previously observed official download records
with first- and last-seen times. Cached records can be viewed while offline;
a historical link still has to answer before a package can be downloaded.
Java acquisition verifies the manifest's version-record hash when available,
the returned version ID, and the server file's published size and SHA-1.
Requests clear unrelated client headers and use the approved public origins.

In the SSH/container content dialog, **List installed content** identifies
eligible files and their current state. Select a package to enable, disable,
update, roll back or remove. Updates stream a replacement and preserve the
previous revision on the target. Removed files stay in a separate recovery
directory. The target rechecks selection identity and modification state.
Bedrock updates match the pack UUID and group and preserve existing world
activation where supported.
