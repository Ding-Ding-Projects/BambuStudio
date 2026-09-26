# Preferences startup and tab navigation verification

The Release build from source commit `b53fad13cdbea61fceb7896f2b926bf9f023908d` produced `BambuStudio.dll` with SHA-256 `E942B5D328E6E8DD4BDC9B4E08AA3534830A3C5362248FD3D50482C3F8403CEA`. It was launched on a hidden Windows desktop with Mesa llvmpipe. The captures use the real application at 96 DPI in the English light theme. The Preferences window measured 780 × 640 pixels.

| State | Image | SHA-256 |
| --- | --- | --- |
| Earlier source `a02c52bbfbe4d5ede2daf4b8d59410b8c4dc3089`: startup stopped on a missing `export` bitmap | [Initialization dialog](before-missing-export.png) | `4FB62FEE5DD51087C4DE2E8A7DC1B0D7471C51CC198A2B3DE5837F6E55224D83` |
| Rebuilt source: Preferences opened on Appearance | [Appearance tab](after-appearance.png) | `0B856BA2EC8AD3428CCED0473EB023156BDC5B07F068315CC1EA47973EB043A9` |
| Rebuilt source: selecting General changed the active tab and content | [General tab](after-general.png) | `341591C224AB68471D93BF0C982C4A742B3ABB21B70D4F8E575567134D21E206` |

The captures were taken through the repository's `run-bambustudio` driver and the Lowlevel cheap headless route, then inspected visually. The before image contains only the initialization dialog; the after images contain only Preferences. No account, recent-file, or private profile content appears in these images. The capture time and timezone were not recorded by the capture route and are unavailable.
