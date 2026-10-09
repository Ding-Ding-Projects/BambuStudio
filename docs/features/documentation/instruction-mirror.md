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

   The refresh runs the privacy scan described below over the whole export, title included, before it writes anything. Any finding refuses the refresh. Pass `--private-terms <file>` to scan for the private term list as well.
4. Run `node scripts/instructions/check-instruction-mirror.mjs --require --source <export.md>`, then run the maintainer's public-boundary scan on `README.md` and `AGENTS.md`.
5. Commit the refreshed files in the same task as the instruction change.

`--check` writes nothing. It exits with status 1 when either file does not carry the mirror that the given export would produce, and with status 0 when both are current.

A Windows checkout may hold `README.md` and `AGENTS.md` with CRLF line endings, and an export may start with a byte-order mark. Both scripts accept either. The mirror body is compared with LF line endings, and a refresh writes each file back in its own line endings, so repeating it changes nothing.

## Privacy and drift guard

`node scripts/instructions/check-instruction-mirror.mjs` checks both files and exits with status 0 when they pass, 1 when they fail and 2 when it is used incorrectly. It fails when:

- a mirrored body no longer matches its recorded SHA-256, because it was edited by hand;
- the label or layout differs from the generated form;
- only one file carries a mirror, or the two files carry different bodies or different source metadata;
- the markers are duplicated, unbalanced or out of order;
- the body contains a private detail;
- with `--source <export.md>`, the mirror is stale against that export.

A repository with no mirror at all is reported as `ABSENT` and passes, unless `--require` is given.

The privacy scan reports the line, column and category of each finding. It never prints the matched text, so a refusal cannot copy a secret into a log. It flags:

- absolute paths outside the repository: drive paths such as `C:\…`, network shares, `file://` paths, and POSIX paths under home, user, mount, volume, temporary and data directories;
- machine names and host inventories: Windows default computer names, host names under private suffixes such as `.local`, `.lan` or `.internal`, hardware addresses, and SSH `Host` and `HostName` entries;
- IPv4 and IPv6 addresses;
- SSH targets, such as `ssh://` URLs, `user@host:path` forms and the target of an `ssh` or `scp` command, and personal account addresses;
- container hosts: `DOCKER_HOST` values, `tcp://` endpoints and `docker -H` hosts;
- tokens in the common provider formats, private key blocks, webhook URLs, credentials written as `password=…` or `api_key: …`, passwords inside URLs, and authorization headers.

Generic references stay allowed, because they name a kind of location rather than a particular one: environment variables such as `%USERPROFILE%` or `$HOME`, `~/` paths, system paths such as `/usr/bin/env`, the loopback and unspecified addresses, the documentation address ranges `192.0.2.0/24`, `198.51.100.0/24` and `203.0.113.0/24`, and the role addresses `noreply@anthropic.com`, `noreply@github.com` and `git@github.com`. Write a four-part version number after a word that says what it is, such as "version", "release" or "upstream", or with a leading `v`, so it is not read as an address.

The private term list stays outside this repository; no term list is committed here. Pass it with `--private-terms <file>` or the environment variable `INSTRUCTION_MIRROR_PRIVATE_TERMS`. The file is either a JSON document, whose string values (never its keys) are the terms, or plain text with one term per line, where blank lines and lines starting with `#` are ignored. Terms match whole words without regard to case and are never printed. An unreadable or empty list fails. Without a list, the term scan is skipped and the output says so. The maintainer's own public-boundary scan remains a required step.

## Current status

No mirror has been published in this repository yet. Publishing one copies text derived from the maintainer's private instructions into this public repository, so it waits for the maintainer to prepare the sanitized export and approve its publication. Until then, `README.md` and `AGENTS.md` carry no mirror block, and the guard reports `ABSENT`. The refresh tooling, the guard, their tests and this procedure are in place for that first publication.

## Verification

Run `node --test ui-md3/tests/instruction-mirror.test.mjs ui-md3/tests/instruction-mirror-guard.test.mjs`. The tests refresh a temporary copy of both files from a synthetic instruction file. They check the label, the recorded revision, date and digest, the identical body in both files, the block placement, repeated refreshes, check mode, and the refusal of malformed input. They feed the privacy scan one synthetic sample for each kind of private detail and a block of ordinary instruction text that must pass. They also cover the term list, the refusal of an unsanitized export, and each kind of drift. The last test runs the guard on this repository itself, so the guard is enforced on the real `README.md` and `AGENTS.md` once a mirror is published. The tests do not read the canonical instructions.
