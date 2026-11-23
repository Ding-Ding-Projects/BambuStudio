# Bambu Studio handoff, 23 November 2026

```text
Repository: Ding-Ding-Projects/BambuStudio, branch main. Read HANDOFF.md, ROADMAP.md and design/graphite-shell/HANDOFF.md first.

Done in this session (built locally with MSBuild; CI built and published release md3-v326 from cd4a64ad1, normal release with Setup.exe, nupkg, RELEASES and SBOM):
- Lubricate mode rewrite (src/libslic3r/GCode/LubricateRoutine.cpp, GCode.cpp, StatusPanel.cpp): the whole tune plays once per cycle in the note range of the Bambu Lab start tunes, with real rests, while the machine stands still; each bed and toolhead stroke is one move; strokes stay inside the area both H2D/H2C toolheads reach; homing again repeats the start G-code's whole lower-bed, park and home window. Checked by compiling the routine alone and reading its H2C output. Not run on a real printer.
- Title bar printer chip (BBLTopbar.cpp, SelectMachinePop.cpp, MainFrame::select_device): shows the selected printer and status, opens the searchable printer list on any page and selects without switching to the Device page. Opening was driven on a hidden desktop; choosing a printer was not (the LAN printers needed an access code).
- Local tools menu: the local tools moved out of File into their own title bar menu, with Every feature first. Driven on a hidden desktop. Menu labels follow a loaded personal wording file; not verified with a current file.
- Device page cards use a 28 px corner; section labels are sentence case at 13 px.
- Design canvas: one page per feature category, a sticky note per board, accent and motion tweaks, new DeviceSwitcher and ToolsMenu boards.

Open:
- Run the new lubricate routine and melodies on a real H2C.
- Choose a printer from the chip with a connected printer.
- Group the Local tools menu as the ToolsMenu board shows, and review the other menus for findability.
- Cold pull every rack hotend without the touchscreen: the printer exposes no documented remote start; a generated G-code routine would need owner approval and a real H2C test.
- The four old branches gap/lan-drop-* and md3-dynamic-color-wip are not merged: the LAN drop branches carry the station key that main deliberately removed; md3-dynamic-color-wip holds unmerged dynamic colour work.
- Update the installed copy to md3-v326 (release is published; the local install was not updated).
```
