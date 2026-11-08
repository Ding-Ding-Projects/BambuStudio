# Extensions

**Surface:** File > Extensions, the command palette (`Ctrl+Shift+F`, type
"Extensions"), the Every feature page, or the Extensions step of Help > Set up
Bambu Studio → `ExtensionsPanel`
(`src/slic3r/GUI/Extensions/ExtensionsPanel.{hpp,cpp}`), a page in the Local
tools tab. The decisions live in modules without wxWidgets: `ExtensionCore`
(manifests, hooks, permissions, consent, requests, answers and the run record),
`ExtensionProcess` (running one program), `ExtensionHost` (the installed
folders, the built-ins and the saved state) and `ExtensionContext` (what a
request contains, and comment lines in exported G-code). `ExtensionRuntime`
connects them to the app with a worker thread.

Design: [design/local-tools-2026-11](../../../design/local-tools-2026-11/README.md),
board Extensions.

An extension is a folder with a `manifest.json` and a program in any language.
Bambu Studio runs the program at the moments it asked for, sends it one JSON
request on standard input and reads one JSON answer from standard output. The
program runs outside Bambu Studio, one run at a time, on a worker thread, so a
slow or broken extension cannot freeze or crash the app.

## Turning one on

Every extension, the built-in ones included, starts off. Turning it on opens a
prompt that lists exactly what it asks for. **Allow exactly this** turns it on
for this version and this list; **Keep off** records that you said no; closing
the prompt records nothing, so it asks again next time.

A new version, or a changed list, turns it off again until you answer the
prompt again, which marks each permission **New** or **Allowed before**. The
master switch, **Extensions on** at the top right, turns every extension off at
once without forgetting what you allowed.

## The page

- **Add from folder** copies a folder you pick (it must hold `manifest.json`)
  into the extensions folder. A folder whose id is already installed asks
  first: **Replace** or keep the installed version, showing both versions and
  their permission counts. A broken manifest says what is wrong and adds
  nothing.
- **Open folder** opens `%APPDATA%\BambuStudio\extensions`. **Reload** reads
  every folder again, for example after you edited a manifest.
- **Author guide** opens this article.
- **Search** matches names, ids, descriptions, hooks, commands and permissions,
  with the shared search field (plain text, or `.*` for a regular expression).
- **Filters:** All, On, Off, Waiting for you, Manifest errors and Built in, each
  with its count.
- **Limits for every run:** the time limit (1 to 60 s, 10 s at first), the
  largest answer (1 to 1024 KB, 64 KB at first) and how many failed runs in a
  row turn an extension off (0 to 20, 3 at first; 0 never does).
- **The last 7 days:** how many runs answered, timed out, were too large, were
  not valid JSON, broke a rule, failed, or were stopped.
- While something runs, the bar says which extension and hook, how many runs
  wait, and offers **Stop**, which ends the run within a tenth of a second and
  drops the queue. A stopped run never counts as a failure.

Each extension has:

| Tab | What it shows |
| --- | --- |
| Overview | Its hooks (a hook this build does not run yet says so), its commands with **Run**, its program and API version, and the table of every hook |
| Permissions | Every permission: Allowed, Asking now, Not allowed: kept off, or Not asked for, and the things that are never sent |
| Settings | Its own settings, one control each (a field for text and numbers, a switch, a choice), saved as you change them; **Back to the defaults** |
| Runs | Its runs, newest first: when, hook or command, time, answer size and result; **Export runs** saves every run of every extension as JSON |
| Manifest | `manifest.json` as it is on disk, and Valid or what is wrong with it |

**Try with a sample** runs it once with a request made of invented values, every
section marked `"sample": true`. Its notes show; comment lines, files,
checklist rows and blocks it answers are reported and never applied. **Open
its folder** and **Remove** (which deletes the folder after asking) are beside
it.

## The built-in extensions

They are Windows PowerShell scripts in `resources/extensions`, installed into
the extensions folder on first use. Bambu Studio updates a built-in only while
its files are as Bambu Studio installed them (a `.builtin.json` marker keeps
their hashes); once you change one, updates stop and the page says so. A
built-in you remove stays removed until you add its folder again.

| Extension | Runs | Does |
| --- | --- | --- |
| Slice report | After slicing; its command **Save a slice report** | A note with the print time, the filament per slot and the layers, plus a warning count. The command answers a Markdown report for you to save |
| Plate checklist | After slicing and before sending | After slicing, a note only when something deserves a second look: slicing warnings, a long print, many filament changes, a slot that needs a lot of filament. Before sending, the same points as checklist rows. Its settings say what counts: a long print from 8 hours, many changes from 30, a lot of filament from 500 g, and Say when all is well (off) for a note even when nothing needs a look |
| G-code time stamp | After G-code export | Comment lines with the export time and the plate's time, filament and layers, at the start and the end of the file |

They run `powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass
-File <script>`. A computer whose policy forbids PowerShell scripts makes them
fail, and the page shows the exit code.

## Hooks

| Hook | When | Receives | May answer | In this build |
| --- | --- | --- | --- | --- |
| `plate.sliced` | After a plate slices; once per slice result, so exporting an already sliced plate does not run it again | `slice`, `printer` | Notes | Runs |
| `gcode.exported` | After plain G-code (`.gcode`) is exported, once the slicing job has finished with the file | `slice` (with the file's name, never its folder), `printer` | Comment lines for the start and the end | Runs |
| `command` | When you start one of its commands: from the page or the command palette | `model` (the selection), `slice` (when the plate is sliced), `printer` | Notes, a text file for you to save | Runs |
| `print.before_send` | When **Send** is pressed in the print dialog for a plate sliced in this project, before the app's own pre-print check | `slice` (every plate added up when all plates are sent), `printer`, `send` | Checklist rows, a block with its reason, notes | Runs |
| `model.imported` | After files are loaded and objects arrive (an import, a drop, a project), once the whole load is done | `model` (only the objects that arrived, with the files' names) | Notes | Runs |
| `print.finished` | When a print Bambu Studio saw running on the printer selected on the Device page finishes, fails or is cancelled (looked at every 3 s, only while an extension is on for it) | `print`, `printer` | Notes | Runs |

A manifest may declare a hook this build does not run yet; it is listed as such
and **Try with a sample** can still exercise it.

## Before a print is sent

Pressing **Send** (or adding the plate to the print queue) in the print dialog
asks every extension that is on for `print.before_send` first, one at a time,
ahead of anything else waiting to run. So does printing a file already on the
printer or a job from the print queue from that dialog, **Send** in Send to the
printer's storage, and **Send** to several printers at once (asked once, before
the file is prepared). The request's `send.source` says which of these it is. With none on, nothing changes: no
window, no delay. Otherwise a window says which extension is checking and for
how long, and offers **Back** and **Send without the checks** (which stops the
run at once; it is recorded as stopped by you and never counts as a failure).
When another extension is still finishing a run it started earlier, the window
says so and waits for it.

When every extension has answered:

| Answers | What happens |
| --- | --- |
| Nothing but rows that pass | **All checks passed** shows for a moment and sending carries on |
| A row that warns or fails, a note, or a run without a valid answer | Everything is shown, grouped by extension, with **Open Extensions**, **Back** and **Send** |
| A valid answer with `"block": true` | The print is not sent. The reasons come first, then every answer, with **Open Extensions** and **Back** only |

Only a valid answer can stop a print, and only from an extension you allowed
`send.block`: an answer that blocks without it breaks a rule, and a run that
times out, fails or answers something invalid is shown as did not answer and
never holds a print back (it still counts toward turning the extension off).
To send a print an extension stopped, change the plate or the filament
mapping, or turn the extension off. **Open Extensions** closes the print
dialog and opens the page at the extension to look at.

After the extensions, the app's own pre-print check follows as before. Each
run is on the Runs tab as `Before sending a print` with the plate, or with the
file's name for a file on the printer or a queue job.

## Permissions

| Permission | What it allows |
| --- | --- |
| `slice.read` | The `slice` section: time, layers, filament per slot, warnings, plate type |
| `model.read` | The `model` section: object names, instance counts, triangle counts, sizes, the selection |
| `printer.read` | The `printer` section: the printer preset, model, nozzle sizes and filament presets; before a print is sent, also the `send` section; after a print ends, the `print` section |
| `notes.show` | Showing notes as notifications |
| `send.warn` | Checklist rows before a print is sent |
| `send.block` | Stopping a print from being sent, with a reason |
| `gcode.comment` | Comment lines in exported G-code |
| `file.save` | Offering a text file you choose where to save |

A section the extension may not receive is left out of the request. Any value
whose name looks like a secret (password, token, access code, cookie, API key
and the like) is removed at every depth, whatever the permissions. An answer
that uses something it was not allowed is thrown away and recorded as broken.

Passwords, access codes, account tokens and the printer's serial number are
never sent. An extension is still a program running as you, so Bambu Studio
cannot stop it from reading your own files: allow only extensions you trust.

## Writing an extension

A folder in `%APPDATA%\BambuStudio\extensions` (or anywhere, then **Add from
folder**):

```
hello/
  manifest.json
  hello.ps1
```

### manifest.json

```json
{
  "id": "com.example.hello",
  "name": "Hello",
  "version": "1.0.0",
  "apiVersion": 1,
  "description": "Says how long the plate takes.",
  "author": "You",
  "run": { "windows": "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File hello.ps1" },
  "hooks": ["plate.sliced"],
  "actions": [{ "id": "greet", "title": "Say hello" }],
  "permissions": ["slice.read", "notes.show"]
}
```

| Field | Rules |
| --- | --- |
| `id` | Required. Lower-case letters, digits, dots and dashes, with at least one dot, like `com.example.hello`; at most 100 characters. Two folders with the same id are both refused |
| `name` | Required, at most 80 characters |
| `version` | Required, `MAJOR.MINOR.PATCH` |
| `apiVersion` | Required, `1`. A newer number than the app supports is refused and the page says which |
| `description` | At most 500 characters |
| `author` | At most 80 characters |
| `run.windows` | Required. The command line, split with Windows rules for quotes and backslashes; at most 1000 characters |
| `hooks` | Hook ids; `command` is added by itself when there are actions |
| `actions` | Up to 10 commands: `id` (lower-case letters, digits and dashes, at most 40) and `title` (at most 60) |
| `permissions` | Permission ids |
| `settings` | Up to 20 settings the person fills in (below) |

Any other field, an unknown hook or an unknown permission makes the manifest
invalid, and the page shows which. A manifest is at most 64 KB.

### Settings

```json
"settings": [
  { "id": "long_print_hours", "title": "A long print from", "description": "Prints this long or longer get a reminder.",
    "type": "number", "default": 8, "min": 1, "max": 72, "whole": true, "unit": "h" },
  { "id": "label", "title": "Label", "type": "text", "default": "", "maxLength": 40 },
  { "id": "loud", "title": "Say when all is well", "type": "switch", "default": false },
  { "id": "mode", "title": "Strictness", "type": "choice", "default": "relaxed",
    "choices": [ { "id": "relaxed", "title": "Relaxed" }, { "id": "strict", "title": "Strict" } ] }
]
```

| Field | Rules |
| --- | --- |
| `id` | Required. Lower-case letters, digits and `_`, at most 40; each once |
| `title` | Required, at most 80 characters |
| `description` | At most 200 characters |
| `type` | Required: `text`, `number`, `switch` or `choice` |
| `default` | Required, and a valid value of the setting |
| `min`, `max`, `whole`, `unit` | `number` only: the range, whole numbers only, and a unit of at most 12 characters shown beside the title |
| `maxLength` | `text` only: 1 to 500 characters, 80 when left out |
| `choices` | `choice` only: 2 to 20 entries with `id` (lower-case letters, digits and dashes) and `title` (at most 60) |

A field of another type (a `unit` on a switch) makes the setting invalid, and
so does an id or title that looks like a password, token, key, access code or
other secret: settings never hold secrets, so such a manifest is refused with
`secret-setting` and the page says which setting.

The person changes them in the extension's Settings tab. Each value is saved as
it changes and checked against the manifest first; a value it refuses is not
saved and the page says why (below the smallest value, not a whole number,
longer than allowed). A newer version that narrows a range or drops a choice
gets its default back for a stored value it no longer allows.

### The program

- There is no shell. The first word of `run.windows` is a file in the folder
  when one has that name, otherwise a program in the Windows system folder (and
  Windows PowerShell's folder), otherwise on `PATH`. A path that leaves the
  folder with `..` is refused.
- It starts in its own folder with a small environment: `SystemRoot`,
  `windir`, `SystemDrive`, `ComSpec`, `PATH`, `PATHEXT`, `TEMP`, `TMP`,
  `USERPROFILE`, `APPDATA`, `LOCALAPPDATA`, `ProgramData`, `ProgramFiles`,
  `ProgramFiles(x86)`, `NUMBER_OF_PROCESSORS`, `PROCESSOR_ARCHITECTURE`, `OS`,
  `PSModulePath`, and `BAMBU_STUDIO_EXTENSION_API=1`. Nothing else the app has
  is inherited, no handle either.
- It reads one request from standard input (UTF-8 JSON; the input is closed
  after it) and writes one answer to standard output (UTF-8 JSON), then exits
  with code 0. Standard error is kept, the first 4 KB, for diagnosis only.
- It is ended when it runs past the time limit or writes more than the largest
  answer, and everything it started ends with it (a Windows job object).

### The request

```json
{
  "apiVersion": 1,
  "hook": "plate.sliced",
  "action": "",
  "extension": { "id": "com.example.hello", "version": "1.0.0" },
  "settings": { "long_print_hours": 8 },
  "context": {
    "slice": {
      "plate": 1, "plateName": "", "seconds": 5025, "prepareSeconds": 360, "layers": 245,
      "bedType": "textured_plate", "filamentChanges": 4,
      "filaments": [{ "slot": 1, "type": "PLA", "color": "#00AE42", "grams": 14.88, "meters": 4.99, "cost": 0.37 }],
      "totalGrams": 14.88, "totalMeters": 4.99, "totalCost": 0.37,
      "warnings": [{ "level": "warning", "code": "", "message": "..." }]
    },
    "printer": { "preset": "...", "model": "...", "nozzles": [0.4], "filamentPresets": ["..."] }
  }
}
```

- `action` is the command's id for `command`, empty otherwise.
- `settings` holds every setting the manifest declares with its value (the
  default when the person has not changed it), and nothing else. It needs no
  permission: these are values the person chose for this extension.
- `slice.file` is the exported file's name for `gcode.exported`.
- When all plates are sent at once, `slice` adds every sliced plate up:
  `plate` is 0, `allPlates` is true, `plateCount` says how many, times,
  layers, filament changes and each slot's filament are totals, `bedType` is
  empty unless every plate shares it, and each warning has its `plate`.
- `send`, for `print.before_send` (with `printer.read`):

  ```json
  "send": {
    "source": "project", "toQueue": false,
    "plate": 1, "allPlates": false,
    "printerModel": "Bambu Lab X1 Carbon", "slicedForThisModel": true,
    "connection": "lan",
    "options": { "bed_leveling": "auto", "flow_cali": "off", "timelapse": "on" },
    "filaments": [
      { "slot": 1, "type": "PLA", "color": "#00AE42", "tray": "A1",
        "trayType": "PLA", "trayColor": "#00AE42", "remainPercent": 61 }
    ]
  }
  ```

  `source` is `project` (a plate of this project to the printer; `toQueue`
  says whether it goes to the print queue instead), `storage` (to the
  printer's storage, not printed), `several` (to several printers at once:
  `printers` says how many and `printerModels` lists each model once, never
  which printers), `printer-file` (a file already on the printer) or `queue`
  (a job from the print queue). The last two carry `file`, the file's or job's
  name without any folder, and no `slice`: they were not sliced in this
  project. `connection` is `cloud` or `lan` (empty when several printers mix
  both). `options` are the options the print dialog shows, with the values it
  uses; Send to storage and several printers have none, and no filament
  mapping. Each filament is a project slot with
  its sliced type and colour and, when it is mapped, the printer's name for
  the slot (`A1`, `HT-A`, `Ext`), what the printer reports there and how much
  is left (`-1` when the printer does not say); an unmapped one has an empty
  `tray`. The printer's own name, serial number, address and access code are
  never part of it.
- `print`, for `print.finished` (with `printer.read`): `result` (`finished`,
  `failed` or `cancelled`), `printerModel`, `seconds` (how long Bambu Studio
  saw it run, or `null`), `seenFromStart` (false when it was already running
  when watching began, so `seconds` is only part of the print), `layers` (as
  the printer reports them, 0 when unknown) and, for a failed print, `error`
  (the printer's code as eight hex digits). When Bambu Studio sent the print
  (printed now, not queued) and saw that printer start within 30 minutes,
  `plannedFilament` holds what the slice planned: `filaments` (slot, type,
  colour, grams, metres), `totalGrams`, `totalMeters` and `complete` (true
  only for a finished print: a failed or cancelled one used less). It is the
  plan, not a measurement, and it is left out for a print that was already
  running when watching began. A print that was already over when watching
  began is never reported, and each ending is reported once. The
  printer's identity is only compared to notice a switch, never sent. Until
  someone has answered an extension's consent prompt, the watcher does not
  even read the extensions folder, so the built-ins are still installed on
  first use.
- `model` holds `plate`, `objects` (up to 200: `name`, `instances`,
  `triangles`, `size` in millimetres, `selected`), `objectCount`,
  `selectedCount` and `triangles`. For `model.imported` it holds only the
  objects that arrived, with `"imported": true`, `files` (the names of the
  files loaded, never their folders, at most 50) and `"units": "mm"`.
- Grams and cost need the filament preset's density and price; without them
  they are 0 and the cost is left out.

### The answer

Every field is optional; an empty object `{}` is a valid answer that shows
nothing.

| Field | Hooks | Permission | Rules |
| --- | --- | --- | --- |
| `apiVersion` | all | | When present, `1` |
| `notes` | all | `notes.show` | Up to 10 strings of at most 300 characters, shown as one notification (before a print is sent, in the check window) |
| `checklist` | `print.before_send` | `send.warn` | Up to 20 rows `{"state": "pass" or "warn" or "fail", "text": ...}`, text at most 200 characters |
| `block`, `blockReason` | `print.before_send` | `send.block` | `true` and a reason of at most 300 characters |
| `gcode` | `gcode.exported` | `gcode.comment` | `{"prepend": [...], "append": [...]}`, each up to 20 lines of at most 200 characters that start with `;` and have no line break |
| `file` | `command` | `file.save` | `{"name": ..., "text": ...}`: a name of at most 80 characters ending in `.txt`, `.md`, `.json` or `.csv`, without `\ / : * ? " < > |`, and at most 64 KB of text |

Comment lines go after Bambu Studio's header block (or at the very start of a
file without one) and after the last line, each extension's lines introduced
by `; added by extension <id> <version>`. The file is written beside the
original as `<name>.partial`, checked for its exact size and then put in place;
when anything goes wrong the exported file stays as it was, and a notification
says why.

A file from a command opens a save dialog with the suggested name; nothing is
written until you choose where.

### hello.ps1

```powershell
$utf8 = [System.Text.UTF8Encoding]::new($false)
$request = [System.IO.StreamReader]::new([Console]::OpenStandardInput(), $utf8).ReadToEnd() | ConvertFrom-Json
$minutes = [math]::Round($request.context.slice.seconds / 60)
$answer = @{ apiVersion = 1; notes = @("Hello: this plate takes about $minutes minutes.") }
$writer = [System.IO.StreamWriter]::new([Console]::OpenStandardOutput(), $utf8)
$writer.Write(($answer | ConvertTo-Json -Compress -Depth 5))
$writer.Flush()
```

Windows PowerShell 5.1 reads a script without a byte order mark in the
system's ANSI code page, so keep scripts ASCII or save them as UTF-8 with a
byte order mark.

Any language works the same way, for example `"run": {"windows": "py.exe -3
hello.py"}` with Python installed, or a program of your own: `"run":
{"windows": "hello.exe"}` with `hello.exe` in the folder.

### Failures

| Result | Meaning |
| --- | --- |
| Answered | A valid answer |
| Ran past its time limit | Ended at the time limit |
| Answer over the limit | It wrote more than the largest answer |
| Not valid JSON | Its output was not one JSON object |
| Broke a rule | A valid JSON answer that broke a rule above; the rule is named, for example `not-allowed:gcode.comment` or `not-comment` |
| Stopped with exit code N | It exited with a code other than 0 |
| Failed | It could not be started, for example `program not found` |
| Stopped by you | **Stop**, or Bambu Studio closing; never a failure |

A failure shows a notification with **Open Extensions**. After the set number
of failures in a row the extension turns itself off, and the page says so;
turning it on again starts the count over.

## Stored values

| Where | What |
| --- | --- |
| `%APPDATA%\BambuStudio\extensions\<folder>` | The installed extensions; a built-in also has `.builtin.json` |
| `%APPDATA%\BambuStudio\extension-state\consent.json` | Per extension: the version you answered for, what you allowed, on or off, failures in a row |
| `%APPDATA%\BambuStudio\extension-state\settings.json` | The master switch, the limits and the built-ins you removed |
| `%APPDATA%\BambuStudio\extension-state\extension-settings.json` | Per extension, the values you gave its settings; only plain text, numbers and true or false are read back. Removing the extension removes them |
| `%APPDATA%\BambuStudio\extension-state\runs.jsonl` | The last 500 runs: time, extension, hook, plate or file name, seconds, answer size, result. Never what was sent or answered |

## Checks

- `tests/extension_core.test.mjs`: manifests and refusals, permission
  enforcement on requests and answers, consent, limits, settings (every
  refusal including secrets, values and their fallback, the request and the
  stored file), the run record, the verdict before a print is sent and real
  processes. Its negative builds never turn a failing extension off, let a
  blocking answer through, or pass a stored setting the manifest no longer
  allows, and must fail.
- `tests/extension_host.test.mjs`: built-ins installed, updated only while
  unchanged and staying removed; adding folders; duplicates and broken
  manifests; consent; the master switch; limits; settings stored, refused,
  carried by every plan, kept across a reload, reset and removed with the
  extension; real runs, failures that turn an extension off, and stopping a
  run. Its negative build overwrites a changed
  built-in and must fail.
- `tests/extension_context.test.mjs`: the request sections (the `send`
  section with every source, file names without folders and several
  printers, the `print` section with the planned filament, and every plate
  added up), the samples and comment lines in G-code (after the header, line
  endings kept, refusals, large files). Its negative builds put the lines
  above the header, or count a file on the printer as sliced here, and must
  fail.
- `tests/extension_import_wiring.test.mjs`: loading files runs
  `model.imported` once the load has returned and objects arrived, nothing
  else calls the inner loader, and the request holds only what arrived; each
  checker must fail on a broken copy.
- `tests/extension_print_watch.test.mjs`: the watcher starts after startup
  and stops at shutdown, looks at nothing while no extension is on for
  `print.finished`, reports each ending once through the narrator's tested
  transition detector, and never sends the printer's identity; each checker
  must fail on a broken copy.
- `tests/extension_before_send_wiring.test.mjs`: the print dialog asks the
  extensions before its own check, for a plate of this project, a file on the
  printer and a queue job (their name only), Send to storage and several
  printers ask before anything is sent and never name the printers, only a
  send sliced here carries the slice, the send facts never read the printer's
  serial number, name, address or access code, the plan is remembered only
  for a print sent now and matched to the same printer within 30 minutes, the
  check runs ahead of anything waiting and reports to its window, and leaving
  the window stops the runs; each checker must fail on a broken copy.
- `tests/extension_builtins.test.mjs`: the built-in manifests and scripts,
  and that the plate checklist's script reads every setting it declares; on
  Windows every built-in really runs under Windows PowerShell through the
  app's own code, the plate checklist answers differently with other
  settings, and a broken copy must be caught.
- `.github/workflows/extensions-windows.yml` runs all seven on a hosted Windows
  machine with MSVC.

## Not done yet

Every declared hook runs, on every way a print is sent. `print.finished`
watches only the printer selected on the Device page, and the filament it
reports is the slice's plan, never what the printer measured. There are no
signed extensions: this project does not sign code, its own or anyone
else's.
