# Rehearsal

- Skill: Stageshow, installed fresh with `npx skills add benjitaylor/skills --skill stageshow` for this pass (the pinned cache was not needed). `SKILL.md` and `references/review.md` were read and the review questions were applied to the rendered frames.
- Closest gold example: `dark-hero` (depth from surface steps on a dark ground, one lit colour, a status on its own pill). Its structure was not copied: these are full application screens, not a marketing still.
- Frames: `frames/Main.png`, `Home.png`, `Preview.png`, `Device.png`, `Dialogs.png`, `Tokens.png`, rendered at 1440 px wide in Chromium with the shipped fonts, after each round of fixes. Dialogs is now 1440 x 900 and Tokens 1440 x 1447, and `canvas.json` has the matching heights.
- Render route: a stand-in for `support.js` kept outside the repository, validated by first rendering the previous artboards and comparing them with the committed frames. It is not the design tool runtime.
- Applied from the skill: the review lens (hierarchy, density, the stated faults such as empty bands, cut controls and uneven insets), real glyphs from the shipped icon font instead of drawn approximations, a quiet palette with one lit colour, and motion with a cause (each animation maps to a native control in `../interface-motion.md`).
- Changed after review: see "Design review" in `HANDOFF.md`.
- Did not apply: cursor choreography, the looping story, `poster`, `film.mjs --gate`, and the 358 px compact redesign, because the screens are application layouts with ambient motion rather than animated product demos. The entrance sequences were checked by reading the end-state frame and a reduced-motion render, not by filming the loop.
- Reduced motion: Main and Preview were rendered with reduced motion emulated; the Preview render was inspected and matches the full-motion end state, the Main render was not inspected.
- Not checked: light theme, Cantonese and bilingual copy, scales other than 100 %, keyboard order, the design tool runtime itself.
