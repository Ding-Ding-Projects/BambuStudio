# Bambu Studio MD3 — legacy Electron shell

This directory retains a thin Electron wrapper around the self-contained `ui-md3` browser concept
for local reference. It is not the native Bambu Studio application, is not built by the maintained
Windows workflow, and is not published on this fork's Releases page.

The maintained installer is generated from the native C++ payload by
[`build_all.yml`](../../.github/workflows/build_all.yml) and
[`build_bambu.yml`](../../.github/workflows/build_bambu.yml). Do not use an Electron package from this
directory as release evidence for the native product.

## Run or package the reference shell on Windows

```powershell
cd ui-md3\desktop
npm install
npm run start
npm run dist
```

`prepare.js` copies `ui-md3/{index.html,landing.html,runtime,app}` into the generated, ignored
`desktop/app/` directory so relative browser paths continue to resolve. `main.js` opens that copy in a
1440 × 900 Electron window. `npm run dist` asks electron-builder to create a per-user NSIS package in
`desktop/dist/`; this is a developer convenience only.

Any locally generated Electron package is unsigned and has no fork release checksum, CycloneDX
inventory, GitHub attestation, immutable-release record, native installer behavior test, or native
visual evidence. The three browser language modes come from the copied `ui-md3/app` resources, but
they do not validate native wxWidgets language/font behavior.
