# Graphite shell feature parity

This inventory lists every user-facing feature the current application exposes on the screens the Graphite shell redesigns, where it lives in the source, where the design now shows it and what changed. The rule for the redesign is that no feature is removed: the design may rearrange, restyle and animate a feature, but each one must stay reachable.

## Summary

| Status | Count |
| --- | --- |
| Present (already drawn) | 88 |
| Restored (was missing, now drawn) | 118 |
| Label fixed (native wording) | 3 |
| Documented (not drawn, still reachable) | 8 |
| Total inventoried | 217 |

Source locations are given at the level of the function, item name or UI string that was read; the inventory was built from the source of this branch and not from a running build, so conditional items (multi-machine, dual extruder, filament manager) are marked as such rather than verified live.

Status values:

- **Present**: drawn in the previous artboards and still drawn.
- **Restored**: missing from the previous artboards and now drawn.
- **Label fixed**: drawn, but under a label that differed from the native string; it now uses the native wording.
- **Documented**: not drawn as a separate element, with the reason in the Notes column (native dropdown contents, conditional states and similar). The feature stays reachable.

Sources were read from this branch: `src/slic3r/GUI/MainFrame.cpp` (`init_tabpanel`, the menu builders), `BBLTopbar.cpp`, `Notebook.cpp`, `Plater.cpp` (Prepare sidebar), `GLCanvas3D.cpp` (scene toolbar), `Gizmos/GLGizmosManager.cpp`, `GUI_Preview.cpp`, `GCodeRenderer/*.cpp`, `StatusPanel.cpp`, `SelectMachine.cpp`, `Melody/MelodyCreatorDialog.cpp`, `CommandPalette.cpp` and `resources/web/homepage3/*.html`.

Artboards: `Main` is Prepare, `Home`, `Preview`, `Device`, `Dialogs`, `TitleBar` and `NavRail` are the shared components.

## Navigation rail

| # | Feature | Source | Design | Status | Notes |
| --- | --- | --- | --- | --- | --- |
| 1 | Home | `MainFrame::init_tabpanel` (`tpHome`) | NavRail | Present | |
| 2 | Prepare | `MainFrame.cpp` (`tp3DEditor`) | NavRail | Present | |
| 3 | Preview | `MainFrame.cpp` (`tpPreview`) | NavRail | Present | |
| 4 | Device | `MainFrame.cpp` (`tpMonitor`, tab text "Monitor", palette destination "Device") | NavRail | Present | The source tab string is "Monitor"; the palette and the design call it Device. Confirm the user-facing label before implementation. |
| 5 | Multi-device | `MainFrame.cpp` (`MultiMachinePage`, only when multi-machine is enabled) | NavRail | Label fixed | Was "Fleet". Conditional destination. |
| 6 | Project | `MainFrame.cpp` (`ProjectPanel`) | NavRail | Present | |
| 7 | Calibration | `MainFrame.cpp` (`CalibrationPanel`) | NavRail | Label fixed | Was "Calibrate". |
| 8 | Ink (filament manager) | `MainFrame.cpp` (`DeviceWebPage`, shown as Ink by the vocabulary map) | NavRail | Label fixed | Was "Inks". Conditional on the filament manager being enabled. |
| 9 | Print (workflow print panel) | `MainFrame.cpp` (`WorkflowPrintPanel`) | NavRail | Restored | New destination tile with the print glyph. |
| 10 | Local tools (service workspace) | `MainFrame::open_service` | NavRail | Restored | Appears only after a local tool is opened; drawn as the last tile. |
| 11 | Settings (navigation action, pinned to the bottom) | `AddNavigationAction` | NavRail | Present | |
| 12 | Workspace | Project workspace panel | NavRail | Present | Design-only shortcut. It has no rail destination in the source; it opens the Project workspace panel. |
| 13 | Overflow when the window is short | `NotebookNavigation::fit()` | NavRail | Documented | Native behaviour; the design assumes the 852 px rail height. |

## Title bar

| # | Feature | Source | Design | Status | Notes |
| --- | --- | --- | --- | --- | --- |
| 14 | Brand tile | `BBLTopbar.cpp` (`m_brand_item`) | TitleBar | Present | The wordmark is in the tile tooltip to save width at 1440 px. |
| 15 | File menu | `m_file_menu_item` | TitleBar | Restored | Dropdown contents are listed under "Menu contents" below. |
| 16 | Edit menu | `m_edit_menu_item` | TitleBar | Restored | |
| 17 | View menu | `m_view_menu_item` | TitleBar | Restored | |
| 18 | Objects menu | `m_objects_menu_item` | TitleBar | Restored | |
| 19 | Calibration menu | `m_calib_item` | TitleBar | Restored | |
| 20 | Help menu | `m_help_menu_item` | TitleBar | Restored | |
| 21 | Project chip with modified dot | `m_title_item` | TitleBar | Present | The design also draws a new-tab button. |
| 22 | Search field with regex access and palette shortcut | `m_search_item` | TitleBar | Present | Hidden below 1180 px, as in the source. |
| 23 | Version history chip (branch, head) | `m_history_item` | TitleBar | Present | |
| 24 | LAN model drop indicator with waiting count | `m_lan_drop_item` | TitleBar | Restored | |
| 25 | Notification bell with unread badge | `m_notification_item` | TitleBar | Restored | |
| 26 | Appearance button | `m_appearance_item` | TitleBar | Restored | |
| 27 | Language switch (EN, Cantonese, bilingual) | `m_language_item` | TitleBar | Present | |
| 28 | Update chip ("Restart to update") | `m_update_item` | TitleBar | Present | Hidden until an update is ready. |
| 29 | Minimize, Maximize, Close | `wxID_ICONIZE_FRAME` and friends | TitleBar | Present | |

### Menu contents

The dropdown menus are native and are not redrawn. Every command below stays in the menu and in the command palette.

| Menu | Commands (source: `MainFrame.cpp`) |
| --- | --- |
| File | New Window, New Project, Open Project, Recent projects, Save Project, Save Project as, Show Project in Folder, Version history, AI filament scanner, Smart home, Melody creator, LAN model drop, Invite someone to send a model, Local model suite, Local file converter, Scheduled settings, Authenticator, Identity history, Local support tickets, Config profiles and backup, Open in External Editor, Import (3MF/STL/STEP/SVG/OBJ/AMF, configs, Model Creator), Export (STL, STLs, 3MF, plate sliced file, all plate sliced files, G-code, toolpaths as OBJ, object list, print statistics, preferences), Quit |
| Edit | Undo, Redo, Cut, Copy, Paste, Delete selected, Delete all, Clone selected, Select all, Deselect all |
| View | Default View, Top, Bottom, Front, Rear, Left, Right, Isometric and Isometric 1 to 3, Fit Camera, Use Perspective or Orthogonal View, Show 3D Navigator, Show Labels by Layer, Show Labels by Object, Show Overhang, Show Wireframe, Enter Camera Full Screen, Set 3DConnexion mouse, Reset Window Layout |
| Objects | Add model, Clone selected, Delete selected, Delete all, Select all, Deselect all |
| Calibration | Temperature, Flow rate (Coarse, Fine), Pressure advance, Retraction test, Max flowrate, VFA, More, Tutorial |
| Help | Status Hub, Offline documentation, Keyboard Shortcuts, Documentation, Dual-extruder slicing tutorial, Dual-extruder TPU tutorial, Setup Wizard, Show Configuration Folder, Show Tip of the Day, Report issue, Check for Update, Check for Presets Update, Open Network Test, What's new, About |

## Prepare

| # | Feature | Source | Design | Status | Notes |
| --- | --- | --- | --- | --- | --- |
| 30 | Printer preset name and state | `Plater.cpp` sidebar | Main (Printer card) | Present | |
| 31 | Edit printer preset | "Click to edit preset" | Main | Restored | Pencil button on the card header. |
| 32 | Sync with the connected printer | `Plater.cpp` sync prompts | Main | Restored | |
| 33 | Printer settings | `Plater.cpp` | Main | Present | |
| 34 | Bed type | `Plater.cpp` ("Bed type") | Main | Present | Was labelled Plate. |
| 35 | Nozzle diameter | `Plater.cpp` | Main | Restored | |
| 36 | Nozzle flow | `Plater.cpp` | Main | Present | |
| 37 | Ink list with slot, preset, temperature | `Plater.cpp` filament section | Main | Present | |
| 38 | Edit an ink | filament combo and edit | Main | Restored | Pencil on every row. |
| 39 | Sync AMS | `Plater.cpp` | Main | Present | |
| 40 | Add ink | "Add filament" | Main | Restored | |
| 41 | Flushing volumes | "Flushing volumes" | Main | Restored | |
| 42 | Ink search with regex access | search field | Main | Present | |
| 43 | Process preset and edit | `Plater.cpp` process section | Main | Restored | Edit button; the preset name is shown. |
| 44 | Global and Objects scope | `Plater.cpp` | Main | Present | |
| 45 | Quality, Strength, Support, Others groups | `Plater.cpp` | Main | Restored | |
| 46 | Advanced process toggle | process advanced mode | Main | Restored | |
| 47 | Express print mode | `Plater.cpp` | Main | Present | |
| 48 | Key process values (walls, infill, support, brim) | `Plater.cpp` | Main | Present | |
| 49 | Section tab strip (Ink, Process, Objects, new) | `TabStrip` prepare sections | Main | Documented | The source shows only the active section's cards. The design shows Ink and Process together and the Objects list on the canvas, so every section is visible at once. |
| 50 | Object manipulation (position, rotation, scale, size, world or object coordinates, keep proportions, reset) | `GizmoObjectManipulation` | Main | Restored | New card under the object list. |
| 51 | Object list | `Plater.cpp` | Main | Present | Search and add part buttons restored. |
| 52 | Move gizmo | `GLGizmoMove3D` | Main | Present | |
| 53 | Rotate gizmo | `GLGizmoRotate3D` | Main | Present | |
| 54 | Scale gizmo | `GLGizmoScale3D` | Main | Present | |
| 55 | Lay on face | `GLGizmoFlatten` | Main | Present | |
| 56 | Cut | `GLGizmoAdvancedCut` | Main | Present | |
| 57 | Mesh boolean | `GLGizmoMeshBoolean` | Main | Restored | |
| 58 | Assembly | `GLGizmoAssembly` | Main | Restored | |
| 59 | Color painting | `GLGizmoMmuSegmentation` | Main | Present | Shown as Paint inks. |
| 60 | Text | `GLGizmoText` | Main | Restored | |
| 61 | SVG | `GLGizmoSVG` | Main | Restored | |
| 62 | Support painting | `GLGizmoFdmSupports` | Main | Present | |
| 63 | Seam painting | `GLGizmoSeam` | Main | Present | |
| 64 | Brim ears | `GLGizmoBrimEars` | Main | Restored | |
| 65 | Fuzzy skin | `GLGizmoFuzzySkin` | Main | Restored | |
| 66 | Measure | `GLGizmoMeasure` | Main | Restored | |
| 67 | Simplify | `GLGizmoSimplify` | Main | Restored | |
| 68 | Add (import models) | `GLCanvas3D` toolbar item `add` | Main | Present | |
| 69 | Add plate | toolbar item `addplate` | Main | Restored | |
| 70 | Auto orient | toolbar item `orient` | Main | Present | |
| 71 | Arrange all and selected plates | toolbar item `arrange` | Main | Present | |
| 72 | Variable layer height | toolbar item `layersediting` | Main | Present | |
| 73 | Split to objects and Split to parts | `splitobjects`, `splitvolumes` | Main | Restored | Split is now a menu button that offers both. |
| 74 | Overview (assembly view) | `assembly_view` | Main | Restored | |
| 75 | More (extend the toolbar) | `More` | Main | Restored | |
| 76 | 3D navigator, perspective and orthogonal, standard views | View menu, `Show 3D Navigator` | Main | Restored | |
| 77 | Plate chips and add plate | `Plater.cpp` plate bar | Main | Present | |
| 78 | Plate settings | "Plate Settings" | Main | Restored | |
| 79 | Estimated time and weight | prepare action bar | Main | Present | |
| 80 | Slice plate | `m_slice_btn` | Main | Present | |
| 81 | Slice options (Slice all) | `m_slice_option_btn` | Main | Restored | Chevron of the split button. |
| 82 | Slice and Send | `m_slice_send_btn` | Main | Restored | Drawn as Send. |
| 83 | Slice and Print | `m_slice_print_btn` | Main | Documented | Same slot as Print plate before slicing; the design shows the sliced state. |
| 84 | Print plate | `m_print_btn` | Main | Present | |
| 85 | Print options (Print all, Send, Send all, Export G-code file, Export plate sliced file, Export all sliced files, Send to Multi-device, Send to Bambu Farm Manager Client) | `m_print_option_btn` | Main | Restored | Chevron of the split button. |
| 86 | Live notifications (for example slicing finished) | `NotificationManager` | Main | Restored | Toast with an action. |

## Preview

| # | Feature | Source | Design | Status | Notes |
| --- | --- | --- | --- | --- | --- |
| 87 | View type: Line Type | `GCodeRenderer` view types | Preview | Present | |
| 88 | View type: Layer Height | | Preview | Restored | |
| 89 | View type: Line Width | | Preview | Restored | |
| 90 | View type: Speed | | Preview | Present | |
| 91 | View type: Fan Speed | | Preview | Restored | |
| 92 | View type: Temperature | | Preview | Present | |
| 93 | View type: Flow (volumetric) | | Preview | Present | |
| 94 | View type: Layer Time | | Preview | Restored | |
| 95 | View type: Filament (Ink) | | Preview | Present | |
| 96 | View type: Thermal Index | | Preview | Restored | In the overflow button. |
| 97 | Show Travel | `GCodeRenderer` options | Preview | Restored | |
| 98 | Show Wipe | | Preview | Restored | |
| 99 | Show Retractions | | Preview | Restored | |
| 100 | Show Deretractions | | Preview | Restored | |
| 101 | Show Seams | | Preview | Restored | |
| 102 | Show Tool changes | | Preview | Restored | |
| 103 | Show Color changes | | Preview | Restored | |
| 104 | Show Print pauses | | Preview | Restored | |
| 105 | Show Custom G-codes | | Preview | Restored | |
| 106 | Legend with per-type time, toggles | `GCodeRenderer` legend | Preview | Present | Percent column added. |
| 107 | Colour scheme selector | "Color Scheme" | Preview | Restored | |
| 108 | Time estimation: prepare, model printing, total | "Time Estimation" | Preview | Restored | Only total time was drawn. |
| 109 | Filament usage: model, flushed, tower, total, cost | "Total Estimation" | Preview | Restored | Only total weight was drawn. |
| 110 | Layer count and color change count | legend summary | Preview | Present | Kept in the summary rows. |
| 111 | View Summary | "View Summary" | Preview | Restored | |
| 112 | Statistics of All Plates | "Statistics of All Plates" | Preview | Restored | |
| 113 | Filament grouping (Regroup, Set to Optimal) | "Filament Grouping" | Preview | Restored | Multi-nozzle printers only. |
| 114 | Layer slider | `IMSlider` | Preview | Present | |
| 115 | Add color change, pause, custom G-code or template at a layer | `IMSlider` context actions | Preview | Restored | Plus button on the slider. |
| 116 | Toggle path view, current layer or all layers | "Toggle path view to current layer/all layers" | Preview | Restored | |
| 117 | Jump to Layer | "Jump to Layer" | Preview | Restored | |
| 118 | Move (horizontal) slider and play or pause | `IMSlider` | Preview | Present | |
| 119 | Simulation speed | "Simulation speed" | Preview | Restored | |
| 120 | Export G-code | `GUI_Preview` actions | Preview | Present | |
| 121 | Send | Print options | Preview | Restored | |
| 122 | Print plate | | Preview | Present | |

## Device

| # | Feature | Source | Design | Status | Notes |
| --- | --- | --- | --- | --- | --- |
| 123 | Live camera view and state | `StatusPanel.cpp` | Device | Present | |
| 124 | Go Live | "Go Live" | Device | Restored | |
| 125 | Camera Full Screen | "Camera Full Screen" | Device | Restored | |
| 126 | Camera Setting | "Camera Setting" | Device | Restored | |
| 127 | Timelapse | "Timelapse" | Device | Restored | |
| 128 | Video | "Video" | Device | Restored | |
| 129 | Chamber light | "Chamber light" | Device | Restored | Toolbar button and a switch in the Fans card. |
| 130 | Printing progress, layer and time left | "Printing Progress" | Device | Present | |
| 131 | Estimated finish time | "Estimated finish time" | Device | Restored | |
| 132 | Pause | "Pause" | Device | Present | The paused state shows Resume in the same slot. |
| 133 | Resume | "Resume" | Device | Documented | Same slot as Pause while paused. |
| 134 | Stop | "Stop" | Device | Present | |
| 135 | Speed levels (Silent, Standard, Sport, Ludicrous) | speed controls | Device | Restored | |
| 136 | Print Options (AI monitoring and related) | "Print Options" | Device | Restored | |
| 137 | Parts Skip | "Parts Skip" | Device | Restored | |
| 138 | Safety Options | "Safety Options" | Device | Restored | |
| 139 | Nozzle, bed and chamber temperatures | "Temperature" | Device | Present | |
| 140 | Set a target temperature | temperature edit | Device | Restored | Edit button on every tile. |
| 141 | Hotends and nozzle information | "Hotends" | Device | Restored | Shown in the Temperature header and under Printer Parts. |
| 142 | Part cooling, Aux and Chamber fans | "Part cooling", "Aux fan" | Device | Present | Now adjustable sliders. |
| 143 | AMS slots with colour, type and remaining | AMS panel | Device | Present | |
| 144 | AMS humidity | "Humidity" | Device | Present | |
| 145 | Dry | "Dry" | Device | Restored | |
| 146 | Load, Unload and Edit a slot | AMS actions | Device | Restored | |
| 147 | Auto Refill | "Auto Refill" | Device | Restored | |
| 148 | External spool slot | AMS panel | Device | Restored | |
| 149 | X and Y motion | "Move" | Device | Present | |
| 150 | Z motion | "Move" | Device | Restored | |
| 151 | Move distance | "Move distance (mm)" | Device | Present | |
| 152 | Auto homing | "Auto homing" | Device | Restored | |
| 153 | Extruder load and unload | "Extruder" | Device | Restored | |
| 154 | Play a melody | `MelodyCreatorDialog` entry | Device | Present | |
| 155 | Hand over to Home Assistant | Smart home | Device | Present | |
| 156 | Printer Parts | "Printer Parts" | Device | Restored | |
| 157 | Storage | "Storage" | Device | Restored | |
| 158 | Calibration (on the device) | "Calibration" | Device | Restored | |
| 159 | Rate this print | "Rate this print" | Device | Documented | Appears after a print finishes; not drawn in the printing state. |
| 160 | Left and right extruder selection | "Left", "Right" | Device | Documented | Dual-extruder printers only. |

## Home

| # | Feature | Source | Design | Status | Notes |
| --- | --- | --- | --- | --- | --- |
| 161 | Open Project | `home.html` | Home | Present | |
| 162 | Create new project | `home.html` | Home | Present | Was "New project". |
| 163 | Recent files with date and revision | `home.html` | Home | Present | |
| 164 | Filter recent files | design addition | Home | Present | |
| 165 | MORE on Recent Files | `home.html` | Home | Restored | |
| 166 | Open in explorer for a file | "open in explorer" | Home | Restored | Shown on hover and focus. |
| 167 | Remove a file from the list | "Remove" | Home | Restored | Shown on hover and focus. |
| 168 | Login, register, log out | `left.html` | Home | Restored | Account block of the Home navigation. |
| 169 | Home search | `left.html` | Home | Restored | With regex access. |
| 170 | Library | `left.html` | Home | Restored | |
| 171 | Recent | `left.html` | Home | Restored | |
| 172 | Online Models | `left.html`, `home.html` | Home | Restored | Navigation entry and a card. |
| 173 | MakerLab | `left.html` | Home | Restored | With the New badge. |
| 174 | Maker's Supply | `left.html` | Home | Restored | With the New badge. |
| 175 | Print History | `left.html`, `home.html` | Home | Restored | Navigation entry and a card with Printing and Success states. |
| 176 | User Manual | `left.html` | Home | Restored | |
| 177 | LAN model drop: Show invite | `home.html` | Home | Restored | |
| 178 | LAN model drop: Open settings | `home.html` | Home | Restored | |
| 179 | LAN model drop: link state, address, QR code | `home.html` | Home | Present | |
| 180 | Copy invite link | design addition | Home | Present | Icon button next to the address. |
| 181 | Printers with state and progress | `home.html` | Home | Present | |
| 182 | Open Device | `home.html` | Home | Restored | |
| 183 | Printers empty state | "No printers yet" | Home | Documented | Shown when no printer is configured. |
| 184 | Melody creator card | `home.html` | Home | Present | |
| 185 | Home Assistant handover | Smart home | Home | Present | |
| 186 | Network error page with Retry | `disconnect.html` | Home | Documented | Separate web page. |

## Dialogs

| # | Feature | Source | Design | Status | Notes |
| --- | --- | --- | --- | --- | --- |
| 187 | Send print job: file, time, weight, printer | `SelectMachine.cpp` | Dialogs | Present | |
| 188 | Printer selector with refresh | printer list | Dialogs | Restored | |
| 189 | Bed type | "Bed type" | Dialogs | Restored | |
| 190 | Express print mode | `SelectMachine.cpp` | Dialogs | Present | |
| 191 | Bed leveling, Flow dynamics calibration | `SelectMachine.cpp` | Dialogs | Present | |
| 192 | Timelapse | | Dialogs | Present | |
| 193 | Use AMS | | Dialogs | Present | |
| 194 | Auto Refill | | Dialogs | Restored | |
| 195 | Ink mapping to AMS slots | AMS mapping | Dialogs | Restored | |
| 196 | Cancel, Send | | Dialogs | Present | |
| 197 | Print (confirm and print) | "Confirm & Print" | Dialogs | Restored | |
| 198 | Melody name | `MelodyCreatorDialog.cpp` | Dialogs | Restored | |
| 199 | Melody preset | "Preset" | Dialogs | Present | |
| 200 | Three-voice step grid | | Dialogs | Present | |
| 201 | Add step, Duplicate, Delete step, Move up, Move down | | Dialogs | Restored | |
| 202 | Open melody | "Open melody" | Dialogs | Restored | |
| 203 | Note, Ticks, Volume | | Dialogs | Present | |
| 204 | Tick length | "How long one firmware tick lasts" | Dialogs | Restored | |
| 205 | Length summary | "%d steps, about %.1f s" | Dialogs | Restored | |
| 206 | Preview, Play on printer | | Dialogs | Present | |
| 207 | Copy G-code | | Dialogs | Present | |
| 208 | Add to start G-code | | Dialogs | Present | |
| 209 | Add to end G-code | | Dialogs | Restored | |
| 210 | Close | | Dialogs | Restored | Close button in the dialog header. |
| 211 | Command palette search | `CommandPalette.cpp` | Dialogs | Present | |
| 212 | Regex search | | Dialogs | Present | |
| 213 | Command results with scope and shortcut | | Dialogs | Present | |
| 214 | Expand or shrink the palette | "Expand the palette to the full window" | Dialogs | Restored | |
| 215 | Settings and documentation results | "Search in settings", "Documentation" | Dialogs | Restored | |
| 216 | Open Preferences | | Dialogs | Restored | |
| 217 | Appearance actions (theme, density, accent) | palette appearance entries | Dialogs | Restored | Accent seed picker is not drawn; theme and density are. |

## Not drawn on purpose

- Native dropdown, context and popup menus (see "Menu contents").
- Modal confirmations, warnings and the tip of the day.
- Conditional printer states: dual extruder, filament track switch, cloud slicing queue, rating after a print.
- Settings, Preferences and the preset editors, which the redesign does not change.
