# Upstream v02.09.01.70 merge captures

Captured on 5 November 2026 (UTC) from the Release build of commit `d2dd1a0cb`, the first build of the
upstream v02.09.01.70 merge that compiled, linked and installed with the one-click build. The capture
used a fresh isolated data directory in bilingual (English and Cantonese) mode, without the network
plugin installed, on a hidden desktop through PrintWindow. `BambuStudio.dll` SHA-256:
`6587a0c539cff19462d67f02e77deecb6c6963348f5f865234b501e9c7f7dcc1`.

| File | State |
| --- | --- |
| `home-bilingual-before-login-fix.png` | Home page before the fix: the bilingual login and register labels stack inside a 36 px row and run into the network plugin notice. This layout predates the merge. |
| `home-bilingual-after-login-fix.png` | The same page with the Home login button as one bilingual group and a growing row, served from the same build with only `left.html` and `left.css` replaced. |
| `setup-wizard-bilingual.png` | First-run setup wizard of the merged build. |
| `prepare-canvas-cube-software-gl.png` | Prepare workspace of the same build with `cube.stl` loaded, rendered by the bundled Mesa llvmpipe software renderer: the build plate with the cube, the scene toolbar, the gizmo rail, the view cube and the object counter. The real-GPU PrintWindow route leaves this pane blank, so this capture used a copy of the install with the Mesa DLLs beside the executable. |

Command-line slicing of the same build: `bambu-studio.exe --slice 0 --outputdir <dir> cube.stl` exits 0
and writes complete G-code (100 layers, 28 min 36 s estimated). On that build, adding `--export-3mf
result.3mf` crashed after writing the G-code, and loading such a 3MF back from the command line crashed
too. Both predate the merge (the same lines are in upstream v02.09.01.70) and are fixed in `0c9734522`,
`9fa304bdb`, `2f745de20` and `dac16c242`: on the build of `dac16c242` the export exits 0 and the 3MF
loads back and slices (100 layers, exit 0).

Opening that 3MF in the GUI first showed an empty plate. The cause is the customized-preset safety
prompt: a 3MF exported without named presets carries an unnamed printer preset, so the GUI asks the user
to confirm the preset G-code is safe (the `-` in the list is the empty name), and the load waits for OK.
Nobody can answer the prompt on a hidden desktop. With the prompt's own "Don't show again" setting
turned on in the capture profile, the same file opens with the cube on the plate. These three captures
come from the Release build of `c5510e983` (`BambuStudio.dll` SHA-256
`4e94f5959abdea819d566186c15e2aecd257e4f1c4e949deef1c39ebd30d4924`), loaded through the layout probe's
`load <path>` command:

| File | State |
| --- | --- |
| `cli-3mf-with-presets-opened.png` | A command-line 3MF exported with the X1 Carbon machine, process and filament presets opens with the cube on the textured plate. |
| `cli-3mf-customized-preset-prompt.png` | The prompt a 3MF exported without named presets raises; its list shows one unnamed preset. |
| `cli-3mf-without-presets-opened.png` | The same preset-less 3MF after the prompt is skipped: project `result`, `cube.stl` on Plate 1. |

Not covered here: slicing from the GUI (the owner-drawn Slice button does not accept a background
click on this route), the device page with a printer, and the other language and theme combinations.
