# Documentation settings in three languages

These page-only images show the locally composed documentation Settings page. The browser used a fresh guest profile on a hidden desktop. The renderer switched the actual language and theme controls after a neutral personal-wording upload, reload and clear. No personal wording was active during capture.

| Reviewed state | Image |
| --- | --- |
| 390 × 1000, English and Hong Kong Cantonese, light theme, emulated mobile | [Settings](390-bilingual_en_yue_HK-light.png) |
| 1440 × 1000, Hong Kong Cantonese, dark theme, desktop | [Settings](1440-yue_HK-dark.png) |

[Read the capture times, exact source and binary-output manifest hash, measured results and limitations](browser-language-evidence.json). At both widths, each of three languages and two themes was exercised. All 12 cases had a page accessibility root, zero unnamed interactive controls and no horizontal document overflow. The run observed one console error, twelve requests outside its own origin and zero uncaught exceptions or failed resources. It verified that the owned processes, ports, desktop and guest profile were gone after teardown.

These measurements do not prove that every string on other pages is translated, or that internal controls never clip. They cover this local build's Settings page only, with screen emulation at scale 1 and without a physical touch device.
