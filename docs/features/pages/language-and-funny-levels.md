# Language modes and funny levels

The site renders in English, Hong Kong Cantonese, or both together, and the tone of each language
is set by its own slider. Copy lives in [`ui-md3/site/copy.js`](../../../ui-md3/site/copy.js);
resolution lives in [`ui-md3/site/core.js`](../../../ui-md3/site/core.js).

## Language modes

The three modes are the repository's canonical set, and the landing page selects them through the
same runtime the interactive prototype uses (`ui-md3/app/i18n.js`), so a mode chosen on the site
survives into the app and back:

| Mode | Renders |
|:---|:---|
| `en` | English only |
| `yue_HK` | Hong Kong Cantonese only |
| `bilingual_en_yue_HK` | English primary with a Cantonese companion |

In bilingual mode a companion of 26 characters or fewer rides inline after the primary label;
anything longer becomes its own quiet second line, so narrow viewports wrap instead of squeezing.
`?lang=yue_HK` forces a mode for a single visit, which is how the layout matrix drives all three.

## Funny levels

Two independent sliders, 1 to 5, persisted separately: one for English, one for Cantonese. Level 1
is fully professional; level 5 is maximum playfulness. English can sit at 1 while Cantonese runs at
5, and the settings surface previews a real string at the level under the slider before it is
committed.

Each entry supplies a tone ladder per language. Ladders may be shorter than five, and the bands
widen to fit:

| Variants | Level 1 | 2 | 3 | 4 | 5 |
|:--|:--|:--|:--|:--|:--|
| 5 | [0] | [1] | [2] | [3] | [4] |
| 4 | [0] | [0] | [1] | [2] | [3] |
| 3 | [0] | [0] | [1] | [2] | [2] |
| 2 | [0] | [0] | [1] | [1] | [1] |
| 1 | [0] | [0] | [0] | [0] | [0] |

A single-variant entry therefore reads identically at every level. That is deliberate for proper
nouns, the legal disclaimer, and atomic control labels such as "Copy" — text with no voice to vary.
Every entry that carries a *message* — prose, warnings, errors, empty states, confirmations,
notifications — has a genuine ladder.

## Voice varies, facts do not

The rule the catalog obeys, and that `ui-md3/tests/site.test.mjs` enforces per variant:

- the unsigned-build warning names *unsigned*, *per user* and *SHA-256* at level 5 exactly as at
  level 1, in both languages;
- the reset confirmation lists what it deletes and says there is no undo at every level;
- the regex error still carries the engine's own message, the timeout still carries the exact
  millisecond budget, and the date error still names `YYYY-MM-DD`;
- a ladder may never change the set of `{placeholders}` a message interpolates, so no variant can
  quietly drop the number, name or file that made the message useful.

## Disclosure

The setting states plainly, in both languages, that the funny level styles **every** message
including errors, warnings and destructive confirmations, and that it never changes what a message
says has happened or what will be affected. Defaults are English 3 and Cantonese 4 — the audience
for a concept redesign expects some personality, not none — and either slider can be moved or reset
at any time.
