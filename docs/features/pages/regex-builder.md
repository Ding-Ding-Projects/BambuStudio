# Regex builder

One implementation in [`ui-md3/site/regex.js`](../../../ui-md3/site/regex.js) serves three roles:
the **Regex lab** tab at full width, the disclosure panel under **every** search bar on the site,
and the matcher those search bars actually filter with.

## Engine and dialect

The engine is the visitor's own browser `RegExp` — ECMAScript, not PCRE, not RE2. The site says so
on the builder itself. Consequences worth knowing:

- flags offered are exactly the ones this engine accepts: `g i m s u y`;
- `\p{Script=Han}` only means a Unicode property under the `u` flag — without it
  ECMAScript's Annex B compiles it silently as the literal characters `p{…}` and reports a clean
  "no matches", which is the worst possible answer because nothing looks wrong. Inserting the
  class chip turns `u` on, and a `\p{…}` pattern without it says which flag is missing;
- named groups use `(?<name> … )` and are shown by name in the results. Group identity comes from
  the engine: a group that did not participate keeps its number and renders as `—` rather than
  being dropped, because dropping it renumbers every group after it;
- literal text inserted through the guided control is escaped with `escapeLiteral`, which covers
  `\ ^ $ . | ? * + ( ) [ ] { } /`, so a typed `.` means a full stop.

## Guided construction

Each part inserts at the caret, or wraps the current selection where wrapping is meaningful:

| Part | Offers |
|:---|:---|
| Literal text | an input whose value is escaped on insert |
| Character class | `\d \D \w \s .` `[a-z] [A-Z]` `\p{Script=Han}` |
| Anchor | `^` `$` `\b` `\B` |
| Group | capturing, non-capturing, named, lookahead, negative lookahead |
| Alternation | `\|` and a wrapped `(?: … \| … )` |
| Quantifier | `*` `+` `?` `{n}` `{n,}` `{n,m}` and a lazy `+?` |

Alongside them: a raw pattern editor, flag checkboxes, a sample-text area, a live status line, the
match list with capture groups and offsets, and Copy / Export / Use-this-pattern / Clear.

Export writes a Markdown file naming the engine, the pattern, the flags, the page it came from and
the sample it was tested against — enough for someone else to reproduce the result.

## Search bars

Every search bar on the site is `createSearchField(...)`: an input, a `.*` toggle, and a builder
button. Plain text is always the default; regex applies only after the toggle is pressed. The two
directions stay synchronised — typing in the field updates the builder's pattern, and applying a
pattern from the builder fills the field, switches it to regex mode, **and brings the builder's
flags with it**, so the filtered results cannot contradict the preview the user just approved. An
invalid pattern reports the engine's message under the field and matches everything, so a
half-typed pattern never empties the list the user is reading.

Filtering is id-based rather than predicate-based: the caller supplies `items() -> [{id, text}]`
and receives the ids that matched. That indirection exists so an opt-in regex can be evaluated
inside the worker instead of on the thread that draws the page.

The search bars wired to it: the tab list, settings, and the changelog. Each supplies its own
surface as the builder's sample text, so the builder is testing against real content.

## Bounds and denial-of-service posture

A user-written pattern can backtrack catastrophically, and a regex engine cannot be interrupted
mid-`exec`. Two bounds apply.

- **Evaluation and search filtering both run in a Web Worker** created from a blob URL and
  **terminated after 400 ms**. A runaway pattern costs a discarded worker, not a frozen page. The
  result says which path ran. A timeout leaves the previous results on screen and explains why they
  did not move — it does not silently show an unfiltered list.
- **Where no worker can be created** (a hardened browser, a `file://` preview), the search bar's
  `.*` toggle is **disabled** and plain text keeps working. Offering an uninterruptible engine on
  the thread that draws the page would be a loaded gun; an honest "not available" is better.
- **The lab's fallback** in that situation is inline matching against a much smaller sample, and it
  **refuses a quantifier nested inside a quantified group** — the `(x+)+` shape — before compiling,
  because a clock cannot interrupt a single `exec()`. Bounding by input is the only bound that
  works there.
- **A worker that fails to start is not a verdict on the pattern.** That case falls back to the
  inline path and says the sandboxed evaluator could not start, quoting what the browser reported,
  rather than reporting a valid pattern as invalid.
- Pattern length is capped at 512 characters, the sample at 20 000 (2 000 inline), matches at 500,
  and a zero-width match advances `lastIndex` so `a*` cannot spin.

Nothing is transmitted. Patterns and sample text are evaluated in the page and never leave it.
