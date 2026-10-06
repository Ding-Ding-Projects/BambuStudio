# Native appearance design handoff

Target: wxWidgets appearance editor and shared owner-drawn controls.
Baseline: b87c23f2, existing checked-in Material control kit.

The available tool catalog has no Material Designer creation, export or handoff capability. The implementation uses the existing checked-in native controls and adds a rendered geometry sample within the existing shape page. This document is a state/implementation handoff, not a screenshot or runtime proof.

States: default theme values; inherited element values; user override; invalid/nonfinite metric; reset to the original sizer flags; narrow preview; retargeted editor; unsupported rich-text capability explanation.

Owned implementation: `src/slic3r/GUI/Appearance/`. Shared Button, Tab, Menu and palette adoption belongs to the integration lane. Preview targets the editor's current stable element ID and subscribes to registry changes. It contains no user images or user-provided text.

Required remaining evidence: application build identity, normal/minimum viewport, English/Cantonese/bilingual, light/dark, 100/125/150/200 percent display scaling, keyboard/screen-reader flow, margin reset, high-padding/high-border stress, before/after and per-click captures. No reference image has been fabricated.
