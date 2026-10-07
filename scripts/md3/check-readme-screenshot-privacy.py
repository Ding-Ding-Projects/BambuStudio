#!/usr/bin/env python3
"""Decide which retaken README screenshots may leave the runner, and stage only those.

The README screenshot workflow uploads plain PNG files from a public repository,
so this check runs before the upload and fails closed. An image is staged only
when every one of these holds:

  - its report row says "done" and its path is on the allowlist (exact match);
  - its text evidence exists and is complete: the layout dump recapture.py took
    right after the capture (--evidence-probe), or the page-text record that
    capture-app.mjs --readme-references writes, ending with {"kind": "end"};
  - no string in that evidence, raw or decoded, contains a Windows user-profile
    path (the USER_PROFILE_PATH pattern of ui-md3/tests/evidence-privacy.test.mjs),
    a POSIX home path, this runner's account or computer name, "runneradmin",
    a GitHub token (gh[pousr]_..., github_pat_) or an email address;
  - the file is a real PNG inside the checkout, within its size bounds, and not
    blank: at least 12 distinct colours and a channel standard deviation of at
    least 10 on a 128-pixel thumbnail (the check Capture-HostedReleaseGui.ps1
    applies to hosted captures).

    python scripts/md3/check-readme-screenshot-privacy.py --report <report.json>
        --evidence-dir <dir> --source-root <checkout> --allowlist-env README_ALLOWLIST
        --out <new upload dir> [--source-commit <sha>] [--release-tag <tag>]
        [--forbid <literal>]...

The upload directory receives the staged PNGs at their repository paths, a
sanitized report.json (row, path, status, sha256, width, height, and a fixed
reason word for a withheld row) and SHA256SUMS. Nothing else is written there:
no evidence, receipt or log. The matched text of a finding is never printed or
recorded, only its category. Exit status: 0 when at least one image is staged,
1 when none is, 2 when the inputs themselves are unusable.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import socket
import sys
from pathlib import Path, PurePosixPath

# ui-md3/tests/evidence-privacy.test.mjs USER_PROFILE_PATH, in Python syntax:
# a drive letter, the Users folder and an account name, in plain, JSON-escaped or
# forward-slash form; the shared Public profile folder is allowed.
USER_PROFILE_PATH = re.compile(r'[A-Za-z]:(?:\\\\|\\|/)Users(?:\\\\|\\|/)(?!Public(?:\\\\|\\|/|"|$))[^\\/"\s]+')
PATTERNS = (
    ('user-profile-path', USER_PROFILE_PATH),
    ('home-path', re.compile(r'(?<![A-Za-z0-9])/(?:home|Users)/[A-Za-z0-9_.-]+')),
    ('token', re.compile(r'gh[pousr]_[A-Za-z0-9]{20,}|github_pat_')),
    ('email', re.compile(r'[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,}')),
)
RUNNER_ACCOUNT = 'runneradmin'
ALLOWED_PATH = re.compile(r'docs/(?:readme-assets|screenshots)/[A-Za-z0-9_./-]+\.png')
# Byte bounds: a whole window or page is at least 10 KiB, a cropped control at
# least 1 KiB (a 147x42 pill is 1.5 KiB); nothing above 32 MiB.
MIN_BYTES = {'page': 10 * 1024, 'pages': 10 * 1024, 'crop-probe': 1024, 'crop-rect': 1024, 'crop-gl': 1024}
MAX_BYTES = 32 * 1024 * 1024
MIN_SIDE, MAX_SIDE = 16, 8192
PNG_SIGNATURE = b'\x89PNG\r\n\x1a\n'


class UnusableInput(Exception):
    pass


def literals_from_environment(extra):
    """(literal, category) for this runner's account and computer names, plus any given literals."""
    candidates = [(RUNNER_ACCOUNT, 'runner-account')]
    candidates += [(os.environ.get(name, ''), 'account-name') for name in ('USERNAME', 'USER')]
    candidates += [(os.environ.get(name, ''), 'computer-name') for name in ('COMPUTERNAME', 'HOSTNAME')]
    try:
        candidates.append((socket.gethostname(), 'computer-name'))
    except OSError:
        pass
    candidates += [(value, 'forbidden-literal') for value in extra]
    found, seen = [], set()
    for value, category in candidates:
        value = (value or '').strip()
        if value and value.lower() not in seen:
            seen.add(value.lower())
            found.append((value, category))
    return found


def literal_pattern(value):
    # A short name would match inside ordinary words; require it to stand alone.
    escaped = re.escape(value)
    if len(value) < 4:
        escaped = r'(?<![A-Za-z0-9])' + escaped + r'(?![A-Za-z0-9])'
    return re.compile(escaped, re.IGNORECASE)


def strings_in(value):
    if isinstance(value, str):
        yield value
    elif isinstance(value, dict):
        for key, item in value.items():
            yield str(key)
            yield from strings_in(item)
    elif isinstance(value, list):
        for item in value:
            yield from strings_in(item)


def read_evidence(path):
    """All text in one evidence file, or a reason word when it cannot be trusted."""
    if path.is_symlink() or not path.is_file():
        return None, 'missing-evidence'
    raw = path.read_text(encoding='utf-8', errors='replace')
    texts = [raw]
    last = None
    for line in raw.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            return None, 'incomplete-evidence'
        texts.extend(strings_in(record))
        last = record
    if not isinstance(last, dict) or last.get('kind') != 'end':
        return None, 'incomplete-evidence'
    return texts, None


def scan(texts, literals):
    """The first finding category in the texts, or None. Never returns the text."""
    for category, pattern in PATTERNS:
        if any(pattern.search(text) for text in texts):
            return category
    for value, category in literals:
        pattern = literal_pattern(value)
        if any(pattern.search(text) for text in texts):
            return category
    return None


def inspect_image(path, kind):
    """(width, height, sha256) of a usable PNG, or a reason word."""
    from PIL import Image, ImageStat  # Pillow, pinned by the workflow

    if path.is_symlink() or not path.is_file():
        return None, 'missing-image'
    size = path.stat().st_size
    if size < MIN_BYTES.get(kind, 10 * 1024) or size > MAX_BYTES:
        return None, 'image-bytes'
    data = path.read_bytes()
    if not data.startswith(PNG_SIGNATURE):
        return None, 'not-png'
    try:
        with Image.open(path) as image:
            if image.format != 'PNG':
                return None, 'not-png'
            width, height = image.size
            if not (MIN_SIDE <= width <= MAX_SIDE and MIN_SIDE <= height <= MAX_SIDE):
                return None, 'image-size'
            rgb = image.convert('RGB')
    except (OSError, ValueError):
        return None, 'not-png'
    rgb.thumbnail((128, 128))
    colours = rgb.getcolors(maxcolors=65536)
    distinct = len(colours) if colours is not None else 65536
    if distinct < 12 or max(ImageStat.Stat(rgb).stddev) < 10:
        return None, 'blank-image'
    return (width, height, hashlib.sha256(data).hexdigest()), None


def load_allowlist(name):
    text = os.environ.get(name, '')
    entries = [line.strip() for line in text.splitlines() if line.strip()]
    if not entries:
        raise UnusableInput(f'the allowlist variable {name} is empty')
    for entry in entries:
        if not ALLOWED_PATH.fullmatch(entry) or '..' in PurePosixPath(entry).parts:
            raise UnusableInput('the allowlist names something other than a PNG under docs/readme-assets or docs/screenshots')
    if len(set(entries)) != len(entries):
        raise UnusableInput('the allowlist repeats a path')
    return set(entries)


def check(args):
    allowlist = load_allowlist(args.allowlist_env)
    source_root = Path(args.source_root).resolve(strict=True)
    evidence_dir = Path(args.evidence_dir).resolve(strict=True)
    out = Path(args.out).resolve()
    if out.exists():
        raise UnusableInput('the upload directory must not exist yet')
    if out == evidence_dir or evidence_dir in out.parents or out in evidence_dir.parents:
        raise UnusableInput('the upload directory must be separate from the evidence directory')
    if args.source_commit and not re.fullmatch(r'[0-9a-f]{40}', args.source_commit):
        raise UnusableInput('the source commit is not a 40-character lowercase hex SHA')
    if args.release_tag and not re.fullmatch(r'md3-v\d+', args.release_tag):
        raise UnusableInput('the release tag is not md3-v<number>')
    try:
        report = json.loads(Path(args.report).read_text(encoding='utf-8'))
    except (OSError, ValueError) as exc:
        raise UnusableInput(f'the capture report is unreadable ({type(exc).__name__})') from exc
    rows = report.get('rows') if isinstance(report, dict) else None
    if not isinstance(rows, list):
        raise UnusableInput('the capture report has no rows list')

    literals = literals_from_environment(args.forbid)
    staged, results, seen = [], [], set()
    for index, row in enumerate(rows):
        row = row if isinstance(row, dict) else {}
        path = row.get('file') if isinstance(row.get('file'), str) else None
        allowed = path in allowlist
        result = {'row': index, 'path': path if allowed else None, 'status': 'withheld',
                  'sha256': None, 'width': None, 'height': None}
        reason = None
        if not allowed:
            reason = 'not-allowlisted'
        elif path in seen:
            reason = 'duplicate'
        elif row.get('status') != 'done':
            reason = 'not-done'
        else:
            evidence_name = row.get('evidence_probe')
            if not isinstance(evidence_name, str) or not evidence_name or Path(evidence_name).name != evidence_name:
                reason = 'missing-evidence'
            else:
                texts, reason = read_evidence(evidence_dir / evidence_name)
                if texts is not None:
                    reason = scan(texts, literals)
            if reason is None:
                image = source_root / path
                if source_root not in image.resolve().parents:
                    reason = 'missing-image'
                else:
                    facts, reason = inspect_image(image, row.get('kind', 'page'))
        if path is not None and allowed:
            seen.add(path)
        if reason is None:
            width, height, digest = facts
            result.update(status='uploaded', sha256=digest, width=width, height=height)
            staged.append((path, source_root / path))
        else:
            result['reason'] = reason
        results.append(result)

    summary = {}
    for result in results:
        key = result['status'] if result['status'] == 'uploaded' else 'withheld: ' + result['reason']
        summary[key] = summary.get(key, 0) + 1
    for key in sorted(summary):
        print(f'{summary[key]:3d}  {key}')
    if not staged:
        print('No image passed the privacy check; nothing is staged for upload.', file=sys.stderr)
        return 1

    out.mkdir(parents=True)
    for path, source in staged:
        target = out / path
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    sanitized = {'schema': 1, 'source_commit': args.source_commit or None, 'release_tag': args.release_tag or None,
                 'staged': len(staged), 'rows': results}
    (out / 'report.json').write_text(json.dumps(sanitized, indent=2) + '\n', encoding='utf-8', newline='\n')
    sums = ''.join(f"{r['sha256']}  {r['path']}\n" for r in results if r['status'] == 'uploaded')
    (out / 'SHA256SUMS').write_text(sums, encoding='utf-8', newline='\n')

    # The staged tree is exactly the images, the report and the checksums.
    expected = {PurePosixPath(p) for p, _ in staged} | {PurePosixPath('report.json'), PurePosixPath('SHA256SUMS')}
    actual = {PurePosixPath(p.relative_to(out).as_posix()) for p in out.rglob('*') if p.is_file() or p.is_symlink()}
    if actual != expected:
        shutil.rmtree(out)
        raise UnusableInput('the upload directory holds something other than the staged files')
    # No path here: a runner path names the account.
    print(f'Staged {len(staged)} of {len(results)} rows for upload.')
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--report', required=True, help='recapture.py --report output, or the capture-app.mjs references report')
    ap.add_argument('--evidence-dir', required=True, help='directory holding the evidence files the report rows name')
    ap.add_argument('--source-root', required=True, help='checkout root the report paths are relative to')
    ap.add_argument('--allowlist-env', required=True, help='environment variable holding one allowlisted PNG path per line')
    ap.add_argument('--out', required=True, help='upload directory to create; must not exist')
    ap.add_argument('--source-commit', default='', help='recorded in report.json')
    ap.add_argument('--release-tag', default='', help='recorded in report.json')
    ap.add_argument('--forbid', action='append', default=[], help='another literal that withholds an image (repeatable)')
    args = ap.parse_args(argv)
    try:
        return check(args)
    except (UnusableInput, OSError) as exc:
        message = str(exc) if isinstance(exc, UnusableInput) else type(exc).__name__
        print(f'check-readme-screenshot-privacy: {message}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
