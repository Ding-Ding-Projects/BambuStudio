# Shared instruction mirror

Agents and contributors working in this repository follow the maintainer's shared agent instructions. The canonical copy of those instructions is private, so this repository keeps a sanitized mirror of them in both `README.md` and `AGENTS.md`. Anyone working here can then read the rules without access to the canonical copy.

The mirror is generated. Nobody edits it by hand: an edit made here is overwritten by the next refresh and never reaches the canonical instructions. Instruction changes are made in the canonical instructions first and then mirrored outward, in the same task that changed them.

## Where the mirror appears

Each file carries exactly one block headed **Shared agent instructions (mirror)**. The block opens with a note that labels it as a mirror, says not to edit it here, and links to this article. The note also records three values:

- the canonical source revision the mirror was exported from;
- the date the mirror was written;
- the SHA-256 of the mirrored body.

In `README.md` the block sits before **Report issue**, and the long instruction text is folded into a collapsed section so the first-reader content stays short. In `AGENTS.md` the block follows the repository-specific rules, which agents read first, and the text is shown directly. Both files carry the same body, byte for byte.

The block is delimited by HTML comments that start with `shared-instructions-mirror:`. A hidden metadata comment repeats the revision, date and digest in a form the scripts read.

## What sanitized means

The mirror keeps the rules and drops everything that identifies where they were written or the infrastructure they were written for. It contains no absolute path outside the repository, no operating-system user name or home directory, no machine name or host inventory, no local-network or remote IP address, no SSH target, no container host, no token and no credential.

Where a rule cannot be stated without a private detail, the export generalizes it: it describes the kind of location or host instead of naming one. A requirement is never dropped silently because it was awkward to sanitize.

The private conversation vocabulary is the one deliberate exception. The canonical instructions require it to be omitted entirely from any mirror in a public repository, including every entry of its section, so the mirror does not generalize it. The existing **Agent conversation vocabulary** section of `AGENTS.md` states the discipline without naming any term.

## Refreshing the mirror

1. Change the canonical instructions first.
2. Export a sanitized copy with the maintainer's sanitizing exporter. Write the export outside this repository and review it before going further. The export omits the vocabulary section and generalizes every remaining private location.
3. From the repository root, run:

   ```text
   node scripts/instructions/refresh-instruction-mirror.mjs --source <export.md> --source-revision <revision>
   ```

   `<revision>` is the canonical revision the export came from, written as 7 to 64 lowercase hexadecimal characters. A leading level-one title in the export is replaced by the mirror heading. Any other level-one heading, or text that already contains mirror markers, is refused and nothing is written. Add `--date YYYY-MM-DD` to record a specific date; otherwise a changed mirror records today's UTC date, and an unchanged one keeps its recorded date.
4. Run the maintainer's public-boundary scan on `README.md` and `AGENTS.md` before committing.
5. Commit the refreshed files in the same task as the instruction change.

`--check` writes nothing. It exits with status 1 when either file does not carry the mirror that the given export would produce, and with status 0 when both are current.

## Current status

No mirror has been published in this repository yet. Publishing one copies text derived from the maintainer's private instructions into this public repository, so it waits for the maintainer to prepare the sanitized export and approve its publication. Until then, `README.md` and `AGENTS.md` carry no mirror block. The refresh tooling, its tests and this procedure are in place for that first publication.

## Verification

Run `node --test ui-md3/tests/instruction-mirror.test.mjs`. The tests refresh a temporary copy of both files from a synthetic instruction file. They check the label, the recorded revision, date and digest, the identical body in both files, the block placement, repeated refreshes, check mode, and the refusal of malformed input. They do not read the canonical instructions.
