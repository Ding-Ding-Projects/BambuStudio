# In-app documentation browser (F1)

**Surface:** `Help > Documentation`, the `F1` key anywhere in the main frame, and every
`Documentation / ...` row of the command palette (`Ctrl+Shift+F`) open `DocsBrowserDialog`
(`src/slic3r/GUI/DocsBrowserDialog.{hpp,cpp}`): a modeless, single-instance Material Design 3
window that shows every feature article of this repository without touching the network.

## Behaviour

- **Everything is bundled at build time.** Every `docs/features/**/*.md` file, including each
  category `README.md` index, is packed into `resources/docs/bundle.json` by
  `cmake/modules/BundleDocs.cmake` (a script-mode CMake run: no Node, no Python). Images an
  article references are mirrored under `resources/docs/assets/` at their `docs/`-relative
  path, so `../../screenshots/pages/x.png` keeps resolving after bundling. The generated
  snapshot is committed, so a build whose configure step is stale still ships data.
- **One shared renderer.** Articles are converted by `src/libslic3r/Markdown.{hpp,cpp}`, a
  small deterministic, wx-free CommonMark-subset renderer (ATX headings with stable slug ids,
  paragraphs, emphasis, inline code, fenced and indented code, ordered/unordered/task lists,
  blockquotes, GitHub pipe tables with alignment, links, images, autolinks). Raw HTML in an
  article is escaped, never passed through. Setext headings, reference-style links, footnotes
  and HTML entities are documented as unsupported and render as literal text. The page CSS is
  generated from the live MD3 colour roles, so the article follows theme and dark mode.
- **Layout.** Left: a `SearchField` (plain-text default, `.*` regex toggle, `tune` full regex
  builder popover, case / whole-word / multiline flags) that filters the category -> article
  tree by **title and body text**; the status line reports `N of M articles match`. Right:
  Back / Forward icon buttons, the article title, and the rendered page in a `wxWebView`
  loaded through `SetPage()` (offline; the WebView2 runtime is the same one the Home tab uses).
- **Links.** A relative link to another `.md` article resolves against the bundle and opens
  inside the browser (`bambudocs://article/<path>#<fragment>`); `#fragment` links scroll within
  the page; a link to a repository file outside `docs/features` (root `README.md`, Postman
  collections) opens the repository page in the system browser; any `http(s)` or `mailto:`
  link leaves the app through the usual open-browser warning dialog. `target=_blank` and new
  window requests take the same route.
- **History.** Back and Forward walk a per-window history; opening an article from the tree,
  the palette or a link pushes an entry, and going back then opening something new discards
  the forward entries, exactly like a browser.
- **Keyboard and assistive technology.** Tab order is search -> tree -> Back -> Forward ->
  article -> Close. `Alt+Left` / `Alt+Right` navigate, `Ctrl+F` focuses the search field,
  `Esc` closes. Every control carries a screen-reader name (`Search documentation`,
  `Documentation articles`, `Back`, `Forward`, `Article`), the rendered page has a `<main>`
  landmark labelled with the article title, and a fragment link moves focus to its heading.
- **Language modes.** All dialog chrome goes through `_L()`; article bodies are shown as
  written (the documentation is authored in English).
- **Fallback.** When the embedded web view cannot be created, the right pane shows an explicit
  message pointing at the docs folder instead of an empty frame.

## Configuration

There is nothing to configure. The dialog remembers nothing between launches beyond the
window position wxWidgets restores; search text and history are per window.

Regenerating the bundle by hand (CI and the normal build do this automatically through the
`docs_bundle` target that `libslic3r_gui` depends on):

```text
cmake -DDOCS_SOURCE=docs -DDOCS_OUTPUT=resources/docs -P cmake/modules/BundleDocs.cmake
```

Commit the resulting `resources/docs/bundle.json` and `resources/docs/assets/**` alongside the
article change.

## Failure modes

| Situation | What the user sees |
|-----------|--------------------|
| `resources/docs/bundle.json` missing beside the executable | Empty tree, status line `No documentation bundle was found beside the application resources.`; a warning is logged. |
| Bundle malformed | Same as missing; the parse error is logged. |
| Article linked but not bundled | The link opens the repository page in the system browser instead of dead-ending. |
| Image missing from `assets/` | The web view shows its broken-image placeholder with the Markdown alt text; `BundleDocs.cmake` warns at build time. |
| WebView2 runtime absent | Explicit fallback message in the article pane; the tree and search keep working. |

## Security considerations

- The renderer escapes every `<`, `>`, `&`, `"` and `'` from article text, so an article cannot
  inject markup or script into the page. Only the renderer's own tags reach the web view.
- The internal `bambudocs://` scheme is intercepted in `wxEVT_WEBVIEW_NAVIGATING` and vetoed;
  only paths present in the bundle can be opened. `http(s)` navigation is always vetoed and
  redirected to the system browser through the warning dialog, so the embedded view never
  loads remote content.
- `file://` access is limited to what the renderer emits: image sources under
  `resources/docs/assets/`. Article links to arbitrary local files are not produced.
- Search runs through the bounded regex evaluator (`BoundedRegex`) with its deadline and
  pattern-size limits, exactly like every other search field.

## Verification

- `tests/markdown` (`markdown_tests`): renderer cases for escaping, headings and slugs, breaks,
  emphasis, code spans, links/images/autolinks and rewrite hooks, fenced and indented code,
  nested and task lists, blockquotes, pipe tables with alignment and escaped pipes,
  determinism, and `plain_text()` for search.
- `tests/docs_bundle` (`docs_bundle_tests`): the completeness guard. It walks
  `docs/features/**/*.md`, compares the set of paths, every title, category, index flag and
  body byte-for-byte against `resources/docs/bundle.json`, checks that every referenced
  relative image exists under `resources/docs/assets/`, renders every article, and proves every
  article-to-article link resolves to a bundled path. A file added to the docs tree without
  regenerating the bundle fails this suite.
- `tests/command_palette` asserts the F1 accelerator lives in the single frame table beside
  `Ctrl+Shift+F` and the numpad chords, and that this article is indexed by the palette.
- Manual: open the app offline, press `F1`, search `regex`, open a result, follow a link to
  another article, press `Alt+Left`, click an `https://` link and confirm the warning dialog.

## Suggested articles

- [Command palette (Ctrl+Shift+F)](command-palette.md): the other route into every article.
- [Regex builder](regex-builder.md): the search field's `.*` toggle and builder popover.
- [Keyboard, assistive, and responsive GUI accessibility](gui-accessibility.md): the rules the
  dialog's tab order and names follow.
- [Native Material Design 3 UI](md3-native-ui.md): the dialog chrome and colour roles reused
  by the rendered page's stylesheet.
