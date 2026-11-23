# Bambu Studio handoff, 23 November 2026

```text
Repository: Ding-Ding-Projects/BambuStudio, branch main. Read HANDOFF.md, ROADMAP.md and design/graphite-shell/HANDOFF.md first.

Done in this session (built locally with MSBuild; CI published md3-v326 from cd4a64ad1 and md3-v327 from 6b7a20714, both normal releases with Setup.exe, nupkg, RELEASES and SBOM):
- Lubricate mode rewrite (src/libslic3r/GCode/LubricateRoutine.cpp, GCode.cpp, StatusPanel.cpp): the whole tune plays once per cycle in the note range of the Bambu Lab start tunes, with real rests, while the machine stands still; each bed and toolhead stroke is one move; strokes stay inside the area both H2D/H2C toolheads reach. Not run on a real printer.
- Title bar printer chip (BBLTopbar.cpp, SelectMachinePop.cpp, MainFrame::select_device): opens the searchable printer list on any page and selects without switching to the Device page. Choosing a printer was not driven (LAN printers need an access code).
- Local tools menu (MainFrame.cpp): Every feature and Local tools first, then four headings that are not commands: Printer, Files and models, Windows and games, Safety and settings. The design board design/graphite-shell/ToolsMenu.dc.html shows the same groups. Built; the menu popup could not be opened on the hidden desktop, so the grouping was not seen in the running app.
- Rack cold pull: the owner decided it is only ever started by hand, as its own maintenance cycle, never part of a print. The existing guide (src/slic3r/GUI/DeviceTab/RackColdPullDialog.cpp, from 38cc75783) already works that way and refuses to start while printing, preparing, calibrating or updating. It is now also in Local tools > Cold pull all rack hotends and in the Every feature catalogue (rack-cold-pull). The docs and the Cantonese twin say so.
- Stale tests fixed: the LAN drop menu check now looks in the Local tools menu, and the switch_project_tab fingerprint in tests/native_shared_controls/atlas_shell_preserved_functions.json matches the code again. Twelve related node test files: 83 of 83 pass (feature_catalogue needs cl.exe, so run it from a Visual Studio developer shell).
- The run-bambustudio driver and press.py now match the window title "Bambu Studio" with a space.
- The installed copy on this PC was updated to md3-v327 (2.9.2040) through its own Squirrel updater from the verified release package; 2.9.2029 is kept for rollback. The updated copy was not launched.

Open:
- Run the new lubricate routine and melodies on a real H2C.
- Choose a printer from the chip with a connected printer.
- See the grouped Local tools menu in the running app: posted clicks reach the title bar strip but no popup opens on the hidden desktop (press.py menus also finds 0 items now), so the menu harness needs a new way to open the owner-drawn popup.
- Verify the rack cold pull guide with a populated H2C rack, including a mounted hotend, a cancelled pull and a disconnect.
- Review the other title bar menus for findability on the design canvas.
- The four old branches gap/lan-drop-* and md3-dynamic-color-wip are not merged: the LAN drop branches carry the station key that main deliberately removed; md3-dynamic-color-wip holds unmerged dynamic colour work.
- An untracked fonts/ folder at the repository root holds seven byte-identical copies of resources/fonts (dated 19 October); it was left untouched and not committed.
```
