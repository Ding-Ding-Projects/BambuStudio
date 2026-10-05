# Offline documentation design handoff

The native implementation uses existing registered application controls: `MD3Dialog`, `SearchField`, `TabCtrl`, `ListBox`, and `MD3HtmlWindow`. The current tool inventory exposes no callable Material Designer creation/export route. This records the capability blocker for the native-kit fallback; it does not assert design parity.

## State inventory

- English article with category results, table of contents, and heading anchor.
- Cantonese article with an existing translation.
- Bilingual article with an existing translation.
- Missing translation with explicit English fallback.
- Plain-text results, regex-builder results, and no matching articles.
- Multiple article tabs, back navigation, and close-tab recovery.
- Trusted embedded image and unavailable external reference.

The default client target is 1100 by 760 DIP; minimum size is 680 by 480 DIP. Verification must cover both sizes, English/Cantonese/bilingual, light/dark, and 100%, 125%, 150%, and 200% display scales. No built captures or layout receipts are claimed by this source handoff. All evidence must bind to the final executable and source commit.
