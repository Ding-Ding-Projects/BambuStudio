# Bambu Studio handoff, 23 November 2026

```text
Repository: Ding-Ding-Projects/BambuStudio, branch main. Read HANDOFF.md, ROADMAP.md and design/graphite-shell/HANDOFF.md first.

Done in this session (built locally with MSBuild; CI published md3-v326 from cd4a64ad1, md3-v327 from 6b7a20714, md3-v329 from a4a98bbee and md3-v330 from b999be6c4, all normal releases with a green Windows build):
- Lubricate mode rewrite, the title bar printer chip and the rack cold pull decision (manual only, its own maintenance cycle, never part of a print) from earlier in the session; see ROADMAP.md.
- Menus: Local tools is grouped under Printer, Files and models, Windows and games, and Safety and settings; Help is grouped under Learn, Set up, Get help, and Updates and about. All group headings have Cantonese. File, Edit, View, Objects and Calibration were reviewed and left as they are. Every menu was opened and captured in the running app on a hidden desktop.
- Bilingual menus: items whose label ends in an ellipsis ("Open Project...") now show their Cantonese (src/slic3r/GUI/Widgets/MD3Menu.cpp looks up the stem). Settings applied again in the same language no longer clear the bilingual registry (BilingualRegistry::enable, LanguageMode.cpp), so items whose English comes from the English wording layer (Check for Updates, About Bambu Studio, AI ink scanner) and mnemonic labels (Show &Overhang) show Cantonese too. A personal wording file now also applies to labels ending in an ellipsis (PersonalVocabulary::display); tests/personal_vocabulary/display_behavior.test.mjs covers it and was seen failing without the fix. The new registry unit test in tests/language_mode/bilingual_registry_tests.cpp was not run (that test project is not generated in this build folder).
- Cold pull all rack hotends (Local tools): without a connected H2C it shows a bilingual dialog on any page saying nothing was sent (captured); with an H2C it opens the existing guide.
- Fonts: the app no longer writes a fonts/ folder into its working directory. Before the data folder is chosen, the bundled fonts are staged in %LOCALAPPDATA%\BambuStudioFonts (src/slic3r/GUI/Widgets/Label.cpp). Verified by launching from the repository root. The seven stray copies that had appeared in the repository root were byte-identical to resources/fonts and were moved out.
- Menu capture route for the run-bambustudio skill: launch with BAMBU_LAYOUT_PROBE=1 (driver.py passes it through and now also treats the main window as ready), then run .claude/skills/run-bambustudio/menupopup.py on the hidden desktop. It must be per-monitor DPI aware or the capture is cropped. Documented in the skill.
- Tests: 107 of 108 pass in the 23 node test files that read the changed sources (personal_vocabulary/display_behavior needs cl.exe, so run it from a Visual Studio developer shell). The one failure, tests/narration/narration_wiring.test.mjs, fails the same way on main (narration strings missing from bbl/i18n/BambuStudio.pot) and was left alone.
- The installed copy on this PC is md3-v330 (2.9.2043), updated through its own Squirrel updater after checking the package SHA1 against RELEASES; not launched.

Open (needs the owner and the printer):
- Add the H2C on the Device page with its access code (only the owner enters it), then: run lubricate mode and its melodies; run the rack cold pull guide with a populated rack, including a mounted hotend, a cancelled pull and a disconnect; choose the printer from the title bar chip.
- The four old branches gap/lan-drop-* and md3-dynamic-color-wip are not merged: the LAN drop branches carry the station key that main deliberately removed; md3-dynamic-color-wip holds unmerged dynamic colour work.
```
