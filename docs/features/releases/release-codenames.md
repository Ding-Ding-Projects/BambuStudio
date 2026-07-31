# Release codenames

Every release gets a Hong Kong dish as its codename: `Bambu Studio MD3 v27 — Water
Chestnut Cake 馬蹄糕`. The roster lives inline in the release step of
[`.github/workflows/build_all.yml`](../../../.github/workflows/build_all.yml), and
[`scripts/ci/Test-ReleaseCodenames.ps1`](../../../scripts/ci/Test-ReleaseCodenames.ps1)
holds it to the contract below on every CI run.

## Behavior

Release numbers are sequential: the publish step counts existing `md3-v<N>` tags and takes
the next integer. The codename is then a pure function of that number, in three regimes:

| Release range | Codename shape | Example |
| --- | --- | --- |
| 1 … 217 | one bare dish | `Har Gow 蝦餃` |
| 218 … 15,624 | style × dish | `Golden Har Gow 黃金蝦餃` |
| beyond | style × dish + serving counter | `Golden Har Gow 黃金蝦餃 2th serving 第2籠` |

**217 dishes × 71 styles**, so the combinatorial range alone covers 15,407 releases before
a serving counter is ever needed. The counter then extends it without bound.

Every entry is bilingual — a Latin name, a space, then the Cantonese name as the final
token. The release step splits on that last space to build the combined form, which is why
the shape is a contract and not a convention.

## Configuration

None. Codenames are derived, never configured. To add dishes or styles, **append** to the
arrays in the workflow and re-run:

```powershell
.\scripts\ci\Test-ReleaseCodenames.ps1 -UpdateBaseline
```

## Failure modes

> [!WARNING]
> **Never insert into the middle of either array, and never reorder or delete.** Codenames
> are assigned by index, so an insertion at position *i* renames every release from
> `v(i+1)` onward. Published releases are immutable, so the workflow would then disagree
> with history that can no longer be corrected.

`Test-ReleaseCodenames.ps1` enforces this by diffing the arrays against
`scripts/ci/codename-roster.json` and failing on any change to an existing index. That
baseline is regenerated only with `-UpdateBaseline`, which is a deliberate act.

Other failures the test catches:

- an entry with no Han characters, or with Han characters in its Latin half — the release
  step's split would produce a malformed title;
- an entry containing an apostrophe, which would terminate the single-quoted PowerShell
  literal it lives in;
- duplicate entries, or two dishes sharing a Han name (they would render identically even
  with different English);
- any release number in the checked horizon producing a repeated or empty codename.

## Security considerations

None. The roster is static literal text, the assignment is arithmetic on a release number,
and nothing here reads user input or network content. The codename is written to
`$GITHUB_ENV` and into the release title and notes only.

## Verification

- `scripts/ci/Test-ReleaseCodenames.ps1`, wired into the `Test Windows release inputs` step
  of `build_bambu.yml`. Latest local run: **217 dishes, 71 styles, 15,674 unique codenames
  verified** across all three assignment regimes, append-only check passed.
- The prefix guarantee was checked directly when the roster grew from 97 to 217 dishes: all
  97 original dishes and 40 original styles are byte-identical at the same indices, and
  index 26 still resolves to `Water Chestnut Cake 馬蹄糕`, matching the published `md3-v27`.

## Related

- [Release splash art](../windows/release-splash-art.md) — the per-release dim sum SVG mark,
  a separate and independently seeded dish library.
