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

Not covered here: the 3D canvas, the device page with a printer, and the other language and theme
combinations.
