# Release splash art (fresh dim sum per release)

Every Windows release ships a one-of-a-kind splash image. The packaged copy of
`resources/images/splash_logo.svg` (94x122 viewBox) is regenerated in CI from
the workflow run number, so each release's installer and portable zip carry
unique dim sum art while the repository's default splash file never changes.

## Behavior

`scripts/ci/New-DimSumSplash.ps1` deterministically composes a flat, simple
dim sum scene in the same hand-drawn style as the default splash mark:

- **Dishes**: 2-4 items picked without replacement from a library of eighteen
  hand-authored SVG fragments — har gow, siu mai with roe, custard bun, egg
  tart, spring roll, pleated bao, turnip cake slice, stylized chicken foot,
  egg waffle wedge, tea cup, cheung fun roll, pineapple bun, lo mai gai parcel,
  taro dumpling, char siu sou, mango pudding, sesame ball, and wonton bowl.
  The library is append-only in practice but not by contract: adding a dish
  changes which items a given seed draws, so art is reproducible for a seed
  only *within one library revision*. Every generated file therefore records
  its seed **and** its dish list in the SVG comment header, so any past mark
  can be identified after the fact.
- **Arrangement**: dishes land in left / right / front slots (front is the
  hero slot at full scale); a smaller back slot appears only when a fourth
  dish is drawn. Draw order is back, left, right, front so overlaps read
  naturally.
- **Steamer**: a 1- or 2-tier bamboo steamer plus chopsticks are always
  present so the mark reads as dim sum at a glance.
- **Steam**: 2-4 wisps with seeded x positions, curvature amplitude, and
  curl direction.
- **Palette**: one of four warm palettes (`toasted-bamboo`, `golden-hour`,
  `rosewood`, `jade-morning`) rotating the bamboo hues, wrapper/bun tints,
  chopstick oranges, and steam grey.

## Determinism

All variation flows from a plain 32-bit linear congruential generator
(Numerical Recipes constants, 5-step warm-up) seeded with `-Seed`. The same
seed always produces byte-identical SVG; the generator uses no `System.Random`,
no time, and no environment input, and numbers are formatted with the
invariant culture. The order of seeded draws (palette, dish count, tier count,
wisp count, dish shuffle, slot shuffle, wisp parameters) is part of the
contract — reordering them changes the art for existing seeds.

The chosen seed is the GitHub Actions `run_number`, which is also the basis of
the release's sequential numbering, so the art is reproducible for any given
release after the fact. The generated SVG embeds a comment recording the seed,
palette, dishes, tier count, and wisp count.

## Where it runs

The step "Compose unique dim sum splash art for this release" in
`.github/workflows/build_bambu.yml` runs after the build/install step has
populated `install-dir` and before the SBOM, portable zip, and NSIS installer
steps, overwriting only
`install-dir\resources\images\splash_logo.svg` (the packaged payload copy).
The SBOM therefore hashes the shipped file, and both artifacts pack the fresh
art. The step logs the chosen palette, dishes, slots, tier count, and wisps.

The repository file `resources/images/splash_logo.svg` stays at its default;
local developer builds use the default mark.

## Reproducing locally

```powershell
.\scripts\ci\New-DimSumSplash.ps1 -Seed 123 -OutPath $env:TEMP\splash-123.svg
```

Use the run number of the release's workflow run as the seed to reproduce that
release's exact splash. The script prints the chosen variants and output byte
size, creates missing parent directories, and refuses to write anything that
does not parse as well-formed XML.

## Failure modes

- The script throws (failing the CI step) if the composed SVG is not
  well-formed XML — the previous payload file is left untouched in that case
  because validation happens before the write.
- If `install-dir` does not exist yet (step ordering broken), the script
  creates the directory and the build would still package the art, but the
  application binary would be missing; the build/install step failing earlier
  is the real gate.

## Security considerations

The generator takes no external input beyond an integer seed and a path, makes
no network calls, and writes a single local file from hand-authored fragments.
No user data, secrets, or downloaded content are involved.

## Verification

Local smoke check (performed at feature landing): seeds 1, 42, and 137
produced well-formed, mutually distinct SVGs (2152, 2762, and 2928 bytes), and
regenerating seed 42 produced a byte-identical file (hash equality). In CI,
the step log records the seed and the chosen variants for each release.
