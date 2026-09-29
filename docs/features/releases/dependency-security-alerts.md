# Dependency security alerts

GitHub Dependabot matches security advisories against the package manifests tracked in this
repository. This article records which manifests exist, which of them feed something users receive,
how an alert is triaged, and what was decided for each alert so far. The tracking issue for the first
full triage is [#47](https://github.com/Ding-Ding-Projects/BambuStudio/issues/47).

## What is scanned, and what each manifest feeds

| Manifest | Package manager | What uses it | Reaches users |
|---|---|---|---|
| `src/slic3r/GUI/DeviceWeb/device_page/package.json` and `pnpm-lock.yaml` | pnpm 10.12.1 | The CMake target `device_page_build` runs `CI=1 pnpm install` and `pnpm run build` with the Node 22.22.2 and pnpm it pins, then copies `dist/` to `resources/web/device_page/dist`, which the Windows app loads. | Only the packages the page imports end up in the bundle (see below). |
| `src/slic3r/GUI/DeviceWeb/device_page/package-lock.json` | npm | Nothing. CMake, every workflow, every script and the page's README use pnpm. Upstream Bambu Studio ships this file, so it is kept to avoid a conflict on every upstream merge. | No |
| `tests/web-e2e/package.json` and `pnpm-lock.yaml` | pnpm | A Playwright end-to-end harness that a person runs by hand against the app. No workflow runs it. | No |
| `resources/web/guide/swiper/…/package.json`, `resources/web/include/swiper/…/package.json` | none | Metadata of the vendored Swiper build; there is no lockfile and nothing installs from them. | The vendored files ship as they are. |
| `ui-md3/desktop/package.json` | none | No lockfile; the GitHub Pages site is static and has no install step. | No |

`dist/` is not tracked. The Windows build and release workflow rebuilds the page on every push, and
because `CI` is set, pnpm refuses to change the lockfile: the build installs exactly what
`pnpm-lock.yaml` says, and a lockfile that disagrees with `package.json` (including its
`pnpm.overrides`) fails the build.

### What the shipped page contains

The page's own source imports `react`, `react-dom`, `react-i18next`, `i18next`, `zustand`,
`@radix-ui/*`, `radix-ui` and `@tanstack/react-router`. On 2026-09-29 the built
`assets/index.js.map` listed 238 sources: 128 from the page's `src/` and `locales/`, and 110 modules
from 36 packages. Those 36 are the Radix UI primitives, `@tanstack/history`, `@tanstack/react-router`,
`@tanstack/react-store`, `@tanstack/router-core`, `@tanstack/store`, `aria-hidden`, `get-nonce`,
`i18next`, `immer`, `react`, `react-dom`, `react-i18next`, `react-remove-scroll`,
`react-remove-scroll-bar`, `react-style-singleton`, `scheduler`, `tslib`, `use-callback-ref`,
`use-sidecar`, `use-sync-external-store` and `zustand`. Build and development tools such as `vite`,
`postcss`, `nanoid`, `eslint`, `vitest`, `undici` and `js-yaml` never appear in it.

## The dependency graph was off

On 2026-09-29 the repository's Insights page reported "Dependency graph is disabled", and the SBOM
export (`gh api repos/Ding-Ding-Projects/BambuStudio/dependency-graph/sbom`) answered 404. No
organization code security configuration is attached to the repository. With the maintainer's
approval it was switched back on the same day at 22:03 UTC with
`gh api -X PUT repos/Ding-Ding-Projects/BambuStudio/vulnerability-alerts`, which enables Dependabot
alerts and the dependency graph and nothing else: secret scanning, push protection and security
updates stayed off. The organization's "GitHub recommended" configuration was deliberately not
attached, because it would also turn on CodeQL code scanning runs, and this repository's workflows
build and release without analysis jobs.

What the graph showed once it was back:

- **Its snapshot was seven weeks old.** Every manifest was "Detected automatically on Aug 11, 2026",
  and 35 minutes after the switch it still listed `js-yaml` 4.3.1, `nanoid` 3.3.17, `undici` 7.29.0,
  `browserslist` 4.28.6 and `baseline-browser-mapping` 2.10.43 for the device page. Every alert raised
  in September was matched against those August lockfiles, which is why #10 and #17 stayed open after
  the patched versions landed and why #2 and #19 could not close as fixed. The snapshot should
  refresh the next time a push changes the device page's manifests.
- **New advisories kept arriving.** Three advisories published that afternoon raised #25, #26 and
  #27 on `undici` 8.9.0 in the unused npm lockfile. GHSA-w293-vg96-wgc3 and GHSA-8436-99hf-9mmv are
  both fixed in 7.29.1 on the 7.x line, the version the pnpm pin already holds. #25 was
  auto-dismissed, and #26 and #27 were dismissed as `not_used`.
- **A short or empty alert list is not evidence of a clean lockfile** until the snapshot has caught
  up. Check the locked versions directly, as described below.

## How to triage an alert

1. **Find the manifest's consumer.** Use the table above. An alert on `package-lock.json` concerns a
   file that nothing installs from.
2. **Ask whether the package ships.** Build the page the way CMake does and list the packages in the
   source map:

   ```bash
   cd src/slic3r/GUI/DeviceWeb/device_page
   CI=1 pnpm install
   pnpm run build
   node -e 'const m=JSON.parse(require("fs").readFileSync("dist/assets/index.js.map","utf8"));const s=new Set();for(const p of m.sources){const r=/node_modules\/(?!\.pnpm)(@[^/]+\/[^/]+|[^/]+)\//.exec(p);if(r)s.add(r[1])}console.log([...s].sort().join("\n"))'
   ```

   Remove `node_modules/` and `dist/` afterwards; both are gitignored. The router plugin may rewrite
   `src/routeTree.gen.ts` with different line endings only; restore it with `git checkout`.
3. **Ask whether the vulnerable code path is reachable.** Find every importer of the package under
   `node_modules/.pnpm` and read how it calls the function the advisory names. A build, lint or test
   tool that only reads this repository's own files cannot be fed hostile input from outside.
4. **Check the locked versions against every advisory, including dismissed ones.** An auto-triage
   rule dismisses low-impact alerts on development-scope packages within a second of their creation
   (#4, #5, #8, #9, #22, #23, #24 and #25 so far), so they never appear in the open count. Compare every
   `name@version` key in each `pnpm-lock.yaml` with the vulnerable ranges that
   `gh api "repos/Ding-Ding-Projects/BambuStudio/dependabot/alerts?per_page=100"` lists for all
   alerts, whatever their state. That is how the `undici` pin below was found.
5. **Update what ships or is reachable, and every pin that holds a vulnerable version.** The
   `pnpm.overrides` block in the page's `package.json` holds security pins (`undici`, `js-yaml`,
   `@babel/core`, `brace-expansion`, `nanoid`). A pin exists to hold a patched version, so when an
   advisory covers the pinned version, move the pin to the first fixed release and regenerate the
   lockfile with the pinned pnpm:

   ```bash
   pnpm install --lockfile-only
   ```

   Then check that the frozen install still passes (`CI=1 pnpm install`), that the page builds, and
   compare `dist/` before and after. For a build-tool bump the files should be identical byte for
   byte.
6. **Dismiss the rest with the reason on the alert.** Use `not_used` when the vulnerable code cannot
   run here and `inaccurate` when the lockfile on `main` does not contain the reported version.
   GitHub limits the dismissal comment to 280 characters, so link the tracking issue for the full
   evidence.

## Triage of 2026-09-29

Seventeen alerts were open (9 high, 8 moderate) on seven packages in four manifests. None of the
flagged packages ships, and no vulnerable code path is reachable from outside the build machine.
All seventeen are now dismissed: 13 as `not_used`, and 4 as `inaccurate` because the lockfile on
`main` no longer contains the flagged version and the graph never rescanned it. The two alerts that
arrived after the graph came back (#26 and #27, see above) are dismissed as `not_used` too.

| Alert | Package | Manifest | Locked version | Decision |
|---|---|---|---|---|
| #2 | `nanoid` (GHSA-2v37-7h3g-55p8) | device page `pnpm-lock.yaml` | 3.3.17, pinned | Pin raised to 3.3.18 in `75fc64c69`; dismissed as `inaccurate`. |
| #19 | `js-yaml` (GHSA-2883-xcg3-v3hh) | device page `pnpm-lock.yaml` | 4.3.1, pinned | Pin raised to 4.3.2 in `75fc64c69`; dismissed as `inaccurate`. |
| #10 | `browserslist` (GHSA-73wf-gq98-2v4g) | device page `pnpm-lock.yaml` | 4.28.8 | Patched since `22151a379`; dismissed as `inaccurate`. |
| #17 | `baseline-browser-mapping` (GHSA-w5vr-8v7q-w6rv) | device page `pnpm-lock.yaml` | 2.11.26 | Patched since `22151a379`; dismissed as `inaccurate`. |
| #12, #14 | `@vitest/mocker`, `vitest` (GHSA-82fw-gwwq-j7x9) | device page `pnpm-lock.yaml` | 3.2.7 | Dismissed as `not_used`. |
| #16 | `vitest` (GHSA-82fw-gwwq-j7x9) | device page `package.json` | `^3.2.7` | Dismissed as `not_used`. |
| #1, #7 | `nanoid` (GHSA-2v37-7h3g-55p8, GHSA-xwg4-73v4-xw9w) | device page `package-lock.json` | 3.3.11 | Dismissed as `not_used`: unused lockfile. |
| #3 | `brace-expansion` (GHSA-3jxr-9vmj-r5cp) | device page `package-lock.json` | 2.1.0 and 1.1.14 | Dismissed as `not_used`: unused lockfile. |
| #6 | `@humanfs/node` (GHSA-p498-v437-472g) | device page `package-lock.json` | 0.16.7 | Dismissed as `not_used`: unused lockfile. |
| #11 | `browserslist` (GHSA-73wf-gq98-2v4g) | device page `package-lock.json` | 4.28.6 | Dismissed as `not_used`: unused lockfile. |
| #13, #15 | `@vitest/mocker`, `vitest` (GHSA-82fw-gwwq-j7x9) | device page `package-lock.json` | 3.2.7 | Dismissed as `not_used`: unused lockfile. |
| #18 | `baseline-browser-mapping` (GHSA-w5vr-8v7q-w6rv) | device page `package-lock.json` | 2.10.43 | Dismissed as `not_used`: unused lockfile. |
| #21 | `js-yaml` (GHSA-2883-xcg3-v3hh) | device page `package-lock.json` | 4.3.0 | Dismissed as `not_used`: unused lockfile. |
| #20 | `js-yaml` (GHSA-2883-xcg3-v3hh) | `tests/web-e2e/pnpm-lock.yaml` | 4.3.0 | Dismissed as `not_used`. |

Why each package is unreachable, checked in the installed sources:

- **`nanoid`**: the only importer is `postcss` inside `vite`, which calls `nanoid/non-secure` with a
  fixed size of 6 while building. The advisories need a caller-controlled size of 2^31 or more passed
  to the pool-based `nanoid(size)`, or a size of 0 passed to `customAlphabet` or `customRandom`.
- **`js-yaml`**: imported only by `@eslint/eslintrc`, which parses YAML only for legacy
  `.eslintrc.yml` files, and by `i18next-parser`, which parses YAML only for `.yml` catalogs or a
  YAML config. The page uses a flat `eslint.config.js`, JSON catalogs and a JavaScript parser
  config, and `tests/web-e2e` also uses a flat `eslint.config.js`.
- **`vitest` and `@vitest/mocker`**: the advisory needs the public `mockerPlugin` or
  `interceptorPlugin` on a Vite dev server's HMR socket, or Vitest browser mode. The page runs one
  jsdom test (`pnpm run test:a11y`) with `vi.mock` hoisting, has no `@vitest/browser`, and registers
  neither plugin. The workflow runs no tests. The fix exists only in vitest 4.1.11.
- **`browserslist`, `baseline-browser-mapping`, `@humanfs/node`, `brace-expansion`**: the pnpm
  lockfile already resolves patched versions (4.28.8, 2.11.26, 0.16.8 and 5.0.9); only the unused
  npm lockfile names the old ones.

### Auto-dismissed alerts, checked as well

- **#24** (`undici`, GHSA-3wwx-pv8p-q78v, fixed in 7.29.1) matched the page's own `pnpm.overrides`
  pin of `undici` 7.29.0. The pin was raised to 7.29.1 in `9eb6ee5d2`. `undici` is only used by development tools,
  jsdom in the local tests and cheerio inside i18next-parser; the vitest tests run on it and pass.
- **#22, #23**: the same advisory on the unused npm lockfile (`undici` 7.28.0 and 8.9.0).
- **#4, #5** (`brace-expansion`, GHSA-rgw5-rvv9-x895) and **#8, #9** (`browserslist`,
  GHSA-c83g-rgw3-j3cx): the page's pnpm lockfile resolves versions outside those ranges.

After these changes, the only locked versions in the page's `pnpm-lock.yaml` that fall inside any
advisory from the alert list are `vitest` and `@vitest/mocker` 3.2.7. In `tests/web-e2e/pnpm-lock.yaml`,
`js-yaml` 4.3.0 (alert #20) and `brace-expansion` 5.0.8 (inside GHSA-rgw5-rvv9-x895, which never
raised an alert there) remain; both are development tools of the manually run harness.

## Failure modes and limits

- **A lockfile out of sync with `pnpm.overrides` fails the Windows build.** Always regenerate
  `pnpm-lock.yaml` with pnpm 10.12.1 after changing an override, and prove `CI=1 pnpm install`
  passes before pushing.
- **Dependabot lags behind the lockfile until the graph re-reads it.** Before acting on a
  `pnpm-lock.yaml` alert, check the version the lockfile really contains.
- **The npm lockfile is not maintained for security.** Its alerts are dismissed as `not_used`. With
  the graph back on, every new advisory for a package in it raises another alert (#25 to #27 on
  2026-09-29); removing the file from this repository would end that. If a build path ever starts
  installing with npm, this decision has to be revisited.
- **Deep paths break a local check.** On Windows, pnpm's patch step changes into the patched
  `minimatch` folder, and Node reads `vite`'s `package.json` imports; both fail with
  `ENAMETOOLONG` or `ERR_PACKAGE_IMPORT_NOT_DEFINED` when the checkout sits deeper than about 180
  characters. Run the check in the repository's own `device_page` folder, as the build does.
- **A DeviceWeb test failed for an unrelated reason until `0a0bb64c1`.** From `68f42a887` on,
  `tests/buildSpoolFromTray.test.ts` failed with `ERR_MODULE_NOT_FOUND` because
  `src/features/filament-manager/constants.ts` imported `../../i18nResources` without a file
  extension, which Node's ESM loader cannot resolve when the tests run with
  `--experimental-strip-types`. `0a0bb64c1` added the `.ts` extension, and all five tests pass since.
- **Another change can turn a pin bump's build red.** Run 36614198573 for `46792f02a` failed in a
  C++ file this work never touched (`AppearanceEditorPopover.cpp`, from `8c1e4a5ab`). Read the failed
  step before blaming the lockfile: the `device_page_build` lines show whether the pnpm step passed.

## Security considerations

The new lockfile entries were checked against the npm registry's published integrity values. No
flagged package reaches the Windows app or the GitHub Pages site, so no installed copy is exposed.
Keeping the security pins current matters mostly for the build machine and for anyone running the
development tools on their own computer.

## Verification

On 2026-09-29, with the Node 22.22.2 and pnpm 10.12.1 that CMake pins, before any pin moved, after
`75fc64c69`, and after the `undici` pin moved:

- `CI=1 pnpm install` (frozen lockfile) passed every time.
- `pnpm run build` passed every time, and all 16 files of `dist/` (4,079,543 bytes) were
  byte-identical across the three builds.
- `pnpm run test:a11y` passed 7 of 7 every time.
- Four of the five dependency-free tests passed every time; `tests/buildSpoolFromTray.test.ts`
  failed every time for the reason above, and passes since `0a0bb64c1`.

The C++ app is built only by the Windows build and release workflow, which runs no tests. On the
hosted Windows runner, the `device_page_build` step of run 36611172274 (`75fc64c69`) and of run
36614198573 (`46792f02a`) logged "Lockfile is up to date, resolution step is skipped", installed with
pnpm 10.12.1 and built the page.

The first releases with the new pins are `md3-v169` (from `087fe6f70`, with the `js-yaml` and
`nanoid` pins) and `md3-v170` (from `784d86ff3`, with all three); `md3-v171` (from `85a1d9e86`) has
all three as well. The CycloneDX inventory published with `md3-v171` (`BambuStudioMD3.cdx.json`)
lists the 16 files of `resources/web/device_page/dist` with the same SHA-256 values as the local build
from before any pin moved, so the page that users install did not change at all. On a hidden desktop,
the unmodified `md3-v171` package also passed the release checks recorded for `md3-v169`, with the
same results
([capture provenance](../../screenshots/md3-everything/README.md#release-md3-v171-checks-the-dependency-pin-release)).
