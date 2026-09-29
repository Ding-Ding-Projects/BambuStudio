# Tooltips

Every tooltip of the Windows app is the Material plain tooltip: InverseSurface behind InverseOn text
(`#2f3036` behind `#f1f0f7` in the light theme, `#e3e2e9` behind `#2f3036` in the dark theme), the
kit's small font, 8 x 4 DIP of padding, and small rounded corners where Windows 11 draws them.

## How

wxWidgets shows every tooltip through one shared Win32 tooltip control
(`wxToolTip::GetToolTipCtrl()`). Drawn with its visual style, that control is the system's pale box
in the system font, whatever the theme. `style_tooltips_md3()` in `src/slic3r/GUI/GUI_App.cpp`
removes the visual style, so the control fills with the colours it is given, then sets the Material
colours, margins and font. `GUI_App` applies it once the main window exists and again in
`force_colors_update()` after every theme change, so switching between light and dark takes effect at
once.

Tooltips the app draws itself, the ImGui tooltips on the 3D canvas and the rich tooltip of the switch
buttons, already use the Material roles.

## Verification

- `node --test ui-md3/tests/tooltips.test.mjs` checks the styling and where it is applied.
- `scripts/md3/check-tooltips.py` runs against a released package on a hidden desktop. It opens
  Smart home, moves the pointer over one of its buttons with a posted `WM_MOUSEMOVE`, captures the
  tooltip that opens and compares its most common colour with the theme's InverseSurface. It fails
  when no tooltip opens or when the colour is the system's.
