# Local tools page

**Surface:** File > Local tools, the command palette (`Ctrl+Shift+F`, type
"Local tools"), Every feature, or the first page of the Local tools tab →
`LocalToolsHubPanel` (`src/slic3r/GUI/FeatureFinder/LocalToolsHubPanel.{hpp,cpp}`).
The rules live in the wx-free `LocalToolsHub`
(`src/slic3r/GUI/FeatureFinder/LocalToolsHub.{hpp,cpp}`); the tools write
Recent activity through `LocalToolsActivity`.

Design: [design/local-tools-2026-11](../../../design/local-tools-2026-11/README.md),
board Main.

## What it shows

Every tool that runs on this computer, as a card: its name, what it holds or
is doing now, the same description as its menu item, where it lives in the
menus, **Open** and **Pin**. Nothing on the page is uploaded.

The Local tools tab appears the first time a tool opens, as before, and this
page is always its first page, whichever tool opened the tab.

The line under a tool's name is read from the tool's own files and state when
the page is shown, when **Read every tool's state again** is clicked and when
a tool finishes something. A tool with nothing to report shows no line; none
is guessed.

| Tool | Line |
| --- | --- |
| Practical setup wizard | Not started, Step 3 of 7 saved, or Finished |
| Base64 toolkit | How many entries its history holds |
| Virtual disks | How many disks are in its library |
| Minecraft servers | How many servers run now out of how many, or how many there are |
| Drawing board | How many plates have a drawing |
| Extensions | How many are on and how many wait for your answer, once any consent prompt has been answered |
| Element locks | How many elements are locked |

## Finding a tool

- The search field matches the name, the description, where it lives, extra
  words and the line under the name ("running" finds running servers). The
  `.*` button switches to a regular expression with the shared regex builder.
  A search covers every tool, whichever chip is selected.
- The chips show the pinned tools, every tool, or one group: Files and data,
  Printer, Windows, Drawing, Games, Extensions, Setup and Safety. Each chip
  counts its tools. The chosen chip is remembered.
- A card that cannot open on this system says why instead of Open, as on
  Every feature.

## Pins

A new installation pins nine tools: Base64 toolkit, Extensions, the setup
wizard, Virtual disks, Minecraft servers, Drawing board, Local file converter,
Local model suite and Authenticator. **Pin** and **Unpin** on any card change
the list; at most 12 tools can be pinned. On the Pinned chip, the arrows on a
card move it earlier or later. The pins and their order are kept in
`local_tools_pinned`; unpinning everything is kept too.

## Recent activity

The column beside the tools (below them in a narrow window) lists the eight
newest things the tools finished, each with the tool, the time ("Today
00:52:41", "Yesterday 23:41:02" or the date) and how it ended when it did not
simply finish. What is written down:

| Tool | When |
| --- | --- |
| Base64 toolkit | Text or a file encoded or decoded, a data: URL built or read; the file name or type, never the payload |
| Virtual disks | Every operation when it ends (attach, detach, create, grow, compact, child, merge, reconnect, new ID, Quick pack) with how it ended |
| Minecraft servers | Started, stopped, restarted on schedule, created, added, backed up, restored, version changed, world generator started, removed |
| Extensions | Each real run answered or not; a sample run is never recorded |
| Drawing board | Each export, with the file name |
| Setup wizard | Setup finished |

**Open history** shows up to 200 entries, newest first, with a search that
uses the regex builder. **Keep recent activity** turns writing on or off; with
it off nothing is written and the column says so. **Clear activity...**
deletes every entry after both keys are turned and the slide completes; the
tools and their own histories are not changed.

Activity is kept in `local-tools-activity.jsonl` in the data folder, one
entry per line, at most 200 (the oldest go first). Control characters are
removed and names longer than 120 bytes are cut at a whole character.

## Checks

- `tests/local_tools_hub.test.mjs` builds and runs
  `tests/local_tools_hub_test.cpp`: every tool is a ready Every feature entry
  that opens a tool, groups and chips, pins (defaults, an empty list kept,
  unknown ids dropped, the limit, toggling and moving), filters, cleaning,
  the file format and the 200-entry limit. A negative build that drops the
  newest entry from a full log must fail. It also checks that each
  description is the tool's own menu help in `MainFrame.cpp`.
- `tests/local_tools_hub_wiring.test.mjs` checks that the page opens first in
  the tab, that File > Local tools opens it, that each tool records what it
  finished (Base64 the name only, Extensions never a sample, setup once),
  that nothing is written while recording is off and that Clear waits for the
  two-key confirmation. Each check also fails on a broken copy.

## Not done yet

- The Local tools tab is still added the first time a tool or this page
  opens; it is not one of the tabs shown at startup.
- Live charts, the converter, the model suite, the authenticator and the
  other tools without a line above do not write Recent activity yet.
