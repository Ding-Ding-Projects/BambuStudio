# Native offline article browser

The native documentation reader bundles every Markdown article under `docs/features` into the executable. It uses the existing Material Design dialog, tab, search, list, and HTML viewport controls. Articles are available without a network connection or an installed source checkout.

## Reading and navigation

Open Offline documentation from the application's help integration. Select an article in the categorized results. Article links open inside the reader, including heading anchors. Each article has a table of contents. Back restores the previous article or anchor. Up to twelve article tabs and one hundred navigation entries are retained while the reader is open. Closing an article tab retains at least one open article.

Search matches titles and article text, including supplied Cantonese translations. The shared search field provides plain text, regular expression mode, case and whole-word options, and its guided regex builder. A no-match state is explicit.

## Languages

English, Cantonese, and bilingual article views are available. Cantonese content comes only from the corresponding `.yue_HK.md` source file. Where that file is absent, the reader identifies the missing translation and displays the English source. It does not invent a translation. The initial selection follows the application's language mode. The article-language override and most recently opened article are persisted in the application configuration.

## Selection and export

Article checkboxes support multiple selections, including Shift ranges and Space-key toggling. Select all matches applies to the current search results; inverse selection applies to that same scope. Changing the search clears the selection. Copy selected articles uses the clipboard. Export selected articles writes UTF-8 Markdown or plain text, with the selected range and encoding stated in the output. Existing files require the save dialog's overwrite confirmation. Export and clipboard failures appear in the reader subtitle without dismissing the current article.

## Trusted content and links

The build-time renderer escapes raw HTML and generates a limited native HTML subset for headings, paragraphs, links, lists, tables, and fenced code. Supported PNG, JPEG, and GIF images inside `docs` are embedded by SHA-256 identity. The native renderer permits reads only for those exact in-memory image identities. It never loads an article URL, arbitrary local file, external image, script, or stylesheet.

Known relative article links become internal routes. Unknown, external, unsupported, or out-of-bundle references display an unavailable-offline label and appear in the generated manifest. This includes links to source code outside the article inventory. The reader never opens a browser automatically. It is not a general-purpose CommonMark renderer; advanced Markdown and raw HTML are not interpreted.

## Build and verification

Run `python scripts/documentation/build_bundle.py` after changing article content. Run `python scripts/documentation/build_bundle.py --check` during the build to reject missing or stale generated output. The generated manifest contains article routes, titles, source SHA-256 values, image identities, and the generated header hash. The generator limits article count, per-article bytes, total article bytes, and embedded image bytes.

Run `python -m unittest discover -s tests/documentation -v` for focused security, formatting, and completeness regression coverage. Native build, actual UI interactions, design parity, and capture evidence are separate requirements and are not established by these Python tests.

## Current integration limits

Application help and command palette integration and the build invocation are maintained by the shell integration layer. The current article formatter is shared within this bundle but is not yet the same rendering engine as the existing web-based tooltip renderer. Full common feature integration, additional faithful export formats, external-editor handoff, and verified built-interface captures remain outstanding. These limitations prevent claiming complete documentation-contract delivery.
