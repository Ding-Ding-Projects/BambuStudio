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

## The dependency graph is disabled

On 2026-09-29 the repository's Insights page reported "Dependency graph is disabled", and the SBOM
export (`gh api repos/Ding-Ding-Projects/BambuStudio/dependency-graph/sbom`) answered 404. No
organization code security configuration is attached to the repository. What that means in
practice:

- **Dependabot did not rescan the lockfiles after pushes.** Alerts #10 and #17 stayed open for three
  days after the 2026-09-26 upstream merge (`22151a379`) brought the patched versions, and alerts #2
  and #19 could not close as fixed after the pins moved in `75fc64c69`.
- **A short or empty alert list is not evidence of a clean lockfile.** Check the locked versions
  directly, as described below.
- Alerts were still being raised on 2026-09-29 until at least 07:27 UTC (#24), so the graph was
  switched off, or stopped updating, recently. Turning it back on is a repository setting for the
  maintainer to decide.

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
   (#4, #5, #8, #9, #22, #23 and #24 so far), so they never appear in the open count. Compare every
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
`main` no longer contains the flagged version and Dependabot cannot rescan it.

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
  pin of `undici` 7.29.0. The pin was raised to 7.29.1. `undici` is only used by development tools,
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
- **Dependabot lags behind the lockfile while the dependency graph is off.** Before acting on a
  `pnpm-lock.yaml` alert, check the version the lockfile really contains.
- **The npm lockfile is not maintained for security.** Its alerts are dismissed as `not_used`. If a
  build path ever starts installing with npm, this decision has to be revisited.
- **Deep paths break a local check.** On Windows, pnpm's patch step changes into the patched
  `minimatch` folder, and Node reads `vite`'s `package.json` imports; both fail with
  `ENAMETOOLONG` or `ERR_PACKAGE_IMPORT_NOT_DEFINED` when the checkout sits deeper than about 180
  characters. Run the check in the repository's own `device_page` folder, as the build does.
- **One DeviceWeb test fails independently of this work.** `tests/buildSpoolFromTray.test.ts` fails
  with `ERR_MODULE_NOT_FOUND` because `src/features/filament-manager/constants.ts` imports
  `../../i18nResources` without a file extension (since `68f42a887`), which Node's ESM loader cannot
  resolve when the tests run with `--experimental-strip-types`.

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
  failed every time for the reason above.

The C++ app is built only by the Windows build and release workflow, which runs no tests.
