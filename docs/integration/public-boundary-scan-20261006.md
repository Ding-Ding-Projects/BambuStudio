# Tracked-source publication scan, 6 October 2026

The inspected source is `24aff6625641545a4fa9770a3dcb0caa3395ef67`.
This is a source-content review, not a review of binaries or external published
records.

| Inventory | Count |
| --- | ---: |
| Tracked entries | 13,128 |
| UTF-8 text files scanned | 11,556 |
| Text bytes scanned | 355,351,691 |
| Binary or non-UTF-8 files excluded | 1,571 |
| Submodule entry, not an ordinary file | 1 |
| Files with lexical candidates | 142 |
| Confirmed violations after contextual review | 0 |

Markdown used the canonical public-content scanner. Other UTF-8 files used the
same current word-boundary dictionary matchers, followed by contextual review
instead of treating code identifiers as prose. Two independent reviewers each
examined 71 candidate files through immutable `git show` reads. The completed
scan had no parser exceptions. An earlier attempt to apply the Markdown parser
to arbitrary source was incomplete and is not counted as a successful scan.

The candidates were ordinary commands and identifiers, memory-allocation and
data-structure terms, literal translations, product and release names, animal
examples, syntax-highlighting data, and documented generic example locations.
No candidate was confirmed as private conversational prose or private machine
detail. This classification does not remove checks on subsequent changes.

The non-file entry is `vendor/lowlevel-computer-use-mcp`, a submodule pinned to
`dec8a543085100da168f56c310aae7c7b1fdbc33`. Its nested contents were not included
in this parent-repository scan. Binary contents, runtime-loaded private input,
external issue/release text, hosted pages and pixels were also outside this
inspection. No broader privacy or runtime claim follows from this result.
