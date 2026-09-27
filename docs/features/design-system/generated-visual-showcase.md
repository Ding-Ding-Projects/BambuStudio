# Generated visual showcase

The interactive Material Design 3 reference and its GitHub Pages landing page share a purpose-built
visual suite under `ui-md3/assets/showcase/`. The suite turns slicer concepts into editorial 3D
artwork while keeping the working interface, localization, and accessibility independent from the
images.

## Shipped assets

| Asset | Surface | Subject |
| --- | --- | --- |
| `hero-studio.webp` | Pages hero and app Home welcome panel | Desktop printer, faceted print, and sliced toolpaths |
| `home.webp` | Pages feature card | Finished prints and a resumable project workspace |
| `prepare.webp` | Pages feature card and recent project | Build-plate arrangement and transform guides |
| `preview.webp` | Pages feature card and recent project | Colored G-code toolpath layers |
| `device.webp` | Pages feature card and recent project | Enclosed printer monitoring |
| `multi-device.webp` | Pages feature card | Coordinated four-printer workshop |
| `project.webp` | Pages feature card | Model pictures, parts, and assembly documents |
| `filament.webp` | Pages feature card | Filament and material library |
| `calibration.webp` | Pages feature card and recent project | Calibration parts and dimensional checks |
| `settings.webp` | Pages feature card | Light/dark surfaces, density, and tonal palette |
| `og-social.webp` | Open Graph and X/Twitter preview | Branded social card with the landing-page headline |

The raster sources were generated as original artwork, reviewed for subject fit and unwanted text,
and encoded as WebP. The complete deployed set is under 700 KiB. Generated source PNGs are not
committed because they add no runtime value.

## Behavior and accessibility

- The landing hero loads eagerly because it is the largest first-viewport visual. Feature images
  decode asynchronously and lazy-load.
- Feature images have concise English alternative text. Recent-project thumbnails and the Home
  welcome art are decorative because adjacent visible copy already names their action.
- Text, buttons, and localization remain HTML. No essential instruction is baked into an image.
- The Home image uses a theme-aware mask so English, Hong Kong Cantonese, and bilingual copy retain
  contrast. Narrow layouts reduce the artwork opacity.
- Motion is disabled when `prefers-reduced-motion` is active.

## Configuration

The landing page references artwork from `./assets/showcase/`. The Pages workflow copies that
directory to the site root while the full app copy retains it under `/app/assets/showcase/`. The
layout assertion checks the landing image paths and the social card before deployment.

## Failure modes

- If an asset is renamed without updating `landing.html` or the Home template, the Pages layout test
  fails for root landing references; the app's modular assembly check catches template drift.
- Social crawlers require an absolute image URL, so the Open Graph metadata uses the canonical
  `ding-ding-projects.github.io` address.
- Images are enhancement only. If an image cannot load, all navigation, actions, and descriptions
  remain usable.

## Verification

From the repository root:

```powershell
node ui-md3/scripts/assemble-index.mjs --check
node --test ui-md3/tests/i18n.test.mjs
```

To reproduce the Pages file layout locally, compose `_site` as the Pages workflow does and run:

```powershell
node ui-md3/tests/assert-pages-layout.mjs _site
```

No Postman artifact applies; this is a static visual surface with no HTTP API.
