# Official catalog snapshots

**Refresh official catalog** in the Models section reads the official Ollama library: the family index, every family's tag listing, and every pagination link on each of those pages. The suite keeps the result as a snapshot so the store stays usable offline and so you can see exactly how complete and how fresh it is.

## Verdicts

Each refresh ends with one verdict:

- **Fully traversed**: every family, tag and page link was fetched and parsed. The official site publishes no total count, so completeness comes from following every link and is not certified.
- **Certified complete**: as above, and every collection also published a total count that matched the entries found across its pages. The current HTML source does not publish totals, so expect **Fully traversed**.
- **Failed**: the refresh stopped. The reason is one of: it was stopped with **Stop current operation**; the official catalog could not be reached (this computer may be offline); a page did not pass validation; or the catalog exceeded the safety limit of 10,000 pages or 200,000 tags.

A failed refresh keeps nothing it read. The shown catalog is never a mixture of a partial refresh and the saved one, and no entry is ever guessed.

## What is recorded

Every fully traversed or certified refresh replaces the saved catalog (`catalog.json` in the suite's data folder) with:

- the verdict;
- the **revision**: a SHA-256 digest over every page's address, response identity (the SHA-256 of the exact bytes received), published count, entries and links, so an identical revision means byte-identical source pages;
- the page, family and tag counts;
- the time of the refresh, which is also the last successful refresh.

Each refresh attempt, successful or not, is recorded separately (`catalog-attempt.json`) with its time, verdict, failure reason and the number of pages read.

## What you see

The line above the model list shows the catalog's verdict with its page, family and tag counts, the first twelve characters of its revision, when it was verified and how long ago. After 24 hours the catalog is shown as **Stale** with a prompt to refresh. The age keeps updating while the suite is open.

When the latest refresh failed after the saved catalog was verified, the line says when and why, and that only the last verified catalog and the installed models are shown. Installed models always remain listed, whether or not any catalog is saved.

## Reading the saved catalog back

When the suite opens, the saved pages are traversed again exactly as a live refresh would traverse them, and the revision is recomputed. A saved catalog with a missing page, an edited entry or a revision that no longer matches is not shown as verified. The suite reports that its saved data is invalid and keeps the file for inspection.

## Verification

`tests/ollama_suite_model/catalog_snapshot_tests.cpp` covers the traversed and certified verdicts, every failure reason, that a failure keeps no entries, revision sensitivity, the saved round trip with tamper and missing-page rejection, the attempt record, and age and staleness arithmetic. `tests/ollama_suite_model/ollama_catalog_wiring.test.mjs` checks that the dialog saves every verified traversal, records attempts separately and shows the localized status.
