# Maintaining Cantonese catalog coverage

Keep `BambuStudio_yue_HK.po` and the derived fields in `coverage.json` in the
same commit. Adding messages without updating all three counts stops both the
native-services build and the release build during catalog compilation.

After editing translations, run these commands from the repository root:

```sh
python bbl/i18n/yue_HK/refresh_coverage.py
python bbl/i18n/yue_HK/compile_translation.py --check
```

The refresh command recalculates `translated_messages`, `categories`, and
`agent_drafted_messages` from the parsed catalog. It preserves review dates,
review status, required flows, tone guidance, and other authored metadata.
Refreshing counts does not constitute a human translation review.

The candidate must pass the existing translation, placeholder, category,
review-status, header, source-membership, minimum-coverage, and compiled
round-trip checks before the metadata is atomically replaced. Invalid input
leaves the existing file unchanged. A current file is not rewritten. The PO
and compiled MO are never written by this command.

Source membership is strict by default. Where the current integration build
explicitly allows entries not yet in the English extraction, use the same
option on both commands:

```sh
python bbl/i18n/yue_HK/refresh_coverage.py --allow-unreferenced
python bbl/i18n/yue_HK/compile_translation.py --allow-unreferenced --check
```

Do not use that option to conceal missing source registrations. A deliberate
`source-pending` entry remains subject to every other validation rule.
Use `--require-complete` for the full English-to-Cantonese coverage audit;
refreshing counts alone does not establish complete localization.
Custom fixtures can specify `--po`, `--source`, and `--coverage`.

Builds continue to validate the committed counts strictly. They do not invoke
this authoring command or silently repair metadata. Review and commit the
resulting metadata diff with the translation changes before retrying a build.

Run the standard-library regression tests from the repository root:

```sh
python -m unittest discover -s bbl/i18n/yue_HK -p 'test_refresh_coverage.py' -v
```

The tests include synthetic messages reproducing the 7709-versus-7712 failure,
independent category and draft-count drift, invalid catalogs, metadata
preservation, repeated refreshes, concurrent edits, and failed replacements.
They do not replace the full native build or runtime language checks.
See `STYLE.md` for translation and review requirements.
