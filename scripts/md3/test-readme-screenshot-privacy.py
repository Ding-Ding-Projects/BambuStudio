"""Fixtures for the README screenshot privacy check: one clean image, then one per leak class.

    python scripts/md3/test-readme-screenshot-privacy.py

Each case builds a disposable checkout with real PNG files, layout-dump-shaped
evidence and a capture report, runs check-readme-screenshot-privacy.py as the
workflow does, and asserts what was staged, what was withheld and why, and
that no finding's text reaches the staged report.
"""

import json
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest

from PIL import Image

CHECKER = Path(__file__).with_name("check-readme-screenshot-privacy.py")
ACCOUNT = "fixtureaccount"
COMPUTER = "FIXTURE-HOST-7"
SECRETS = ("someone", ACCOUNT, COMPUTER.lower(), "runneradmin", "ghp_", "github_pat_", "person@example.org")


def dump(*labels, end=True):
    records = [{"kind": "header", "tag": "en-light-comfortable"},
               {"kind": "toplevel", "hwnd": 1, "class": "wxFrame", "title": "Bambu Studio"}]
    records += [{"kind": "window", "hwnd": 10 + i, "parent": 1, "top": 1, "class": "wxStaticText",
                 "name": "staticText", "label": label} for i, label in enumerate(labels)]
    if end:
        records.append({"kind": "end"})
    return "".join(json.dumps(r) + "\n" for r in records)


def busy_png(path, size=(400, 300), seed=1):
    rng = random.Random(seed)
    image = Image.new("RGB", size, (250, 250, 250))
    pixels = image.load()
    for y in range(size[1]):
        for x in range(size[0]):
            if (x // 7 + y // 5) % 3 == 0:
                pixels[x, y] = (rng.randrange(256), rng.randrange(256), rng.randrange(256))
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, "PNG")


class PrivacyCheck(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        self.checkout, self.evidence, self.out = root / "checkout", root / "evidence", root / "upload"
        self.evidence.mkdir()
        self.rows, self.allowlist = [], []

    def tearDown(self):
        self.tmp.cleanup()

    def add(self, name, evidence, kind="page", status="done", allowlisted=True, image="busy"):
        path = f"docs/readme-assets/{name}.png"
        target = self.checkout / path
        if image == "busy":
            busy_png(target, seed=len(self.rows) + 1)
        elif image == "blank":
            target.parent.mkdir(parents=True, exist_ok=True)
            Image.new("RGB", (1200, 800), (255, 255, 255)).save(target, "PNG")
        elif image == "text":
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b"not an image" * 2000)
        row = {"file": path, "kind": kind, "status": status}
        if evidence is not None:
            evidence_name = f"recapture-1-{len(self.rows) + 1}.jsonl"
            (self.evidence / evidence_name).write_text(evidence, encoding="utf-8")
            row["evidence_probe"] = evidence_name
        self.rows.append(row)
        if allowlisted:
            self.allowlist.append(path)
        return len(self.rows) - 1

    def run_check(self):
        report = Path(self.tmp.name) / "report.json"
        report.write_text(json.dumps({"rows": self.rows}), encoding="utf-8")
        env = {k: v for k, v in os.environ.items() if k not in ("USER", "HOSTNAME")}
        env.update(USERNAME=ACCOUNT, COMPUTERNAME=COMPUTER, README_ALLOWLIST="\n".join(self.allowlist) + "\n")
        result = subprocess.run(
            [sys.executable, str(CHECKER), "--report", str(report), "--evidence-dir", str(self.evidence),
             "--source-root", str(self.checkout), "--allowlist-env", "README_ALLOWLIST", "--out", str(self.out),
             "--source-commit", "0" * 40, "--release-tag", "md3-v1"],
            capture_output=True, text=True, env=env, timeout=120)
        for secret in SECRETS:
            self.assertNotIn(secret, result.stdout + result.stderr, "a finding's text reached the log")
        staged = json.loads((self.out / "report.json").read_text(encoding="utf-8")) if self.out.exists() else None
        return result.returncode, staged

    def test_clean_image_is_staged_with_report_and_checksums_only(self):
        self.add("clean", dump("Prepare", r"C:\Users\Public\bbsdd\en-light-comfortable", "D:\\a\\_temp\\bbsdd"))
        code, staged = self.run_check()
        self.assertEqual(code, 0)
        self.assertEqual(staged["rows"][0]["status"], "uploaded")
        self.assertEqual((staged["rows"][0]["width"], staged["rows"][0]["height"]), (400, 300))
        files = sorted(p.relative_to(self.out).as_posix() for p in self.out.rglob("*") if p.is_file())
        self.assertEqual(files, ["SHA256SUMS", "docs/readme-assets/clean.png", "report.json"])
        sums = (self.out / "SHA256SUMS").read_text(encoding="utf-8")
        self.assertEqual(sums, f"{staged['rows'][0]['sha256']}  docs/readme-assets/clean.png\n")

    def test_every_leak_class_withholds_its_image(self):
        cases = {
            "user-profile-path": dump(r"Download folder C:\Users\someone\Downloads"),
            "user-profile-path-json": dump("C:\\\\Users\\\\someone\\\\AppData"),
            "user-profile-path-slash": dump("c:/Users/someone/AppData"),
            "home-path": dump("/home/someone/.config"),
            "account-name": dump(f"Signed in as {ACCOUNT.upper()}"),
            "computer-name": dump(f"Host {COMPUTER}"),
            "runner-account": dump("RunnerAdmin"),
            "token": dump("ghp_" + "A1b2C3d4E5f6G7h8I9j0K1"),
            "token-pat": dump("github_pat_x"),
            "email": dump("Contact person@example.org"),
            # JSON-escaped in the file, so only the decoded text shows the address.
            "email-escaped": '{"kind": "window", "label": "person\\u0040example.org"}\n{"kind": "end"}\n',
            "missing-evidence": None,
            "incomplete-evidence": dump("Prepare", end=False),
        }
        index = {name: self.add(name, evidence) for name, evidence in cases.items()}
        clean = self.add("still-clean", dump("Preview"))
        code, staged = self.run_check()
        self.assertEqual(code, 0)
        reasons = {r["row"]: r.get("reason") for r in staged["rows"]}
        expected = {"user-profile-path-json": "user-profile-path", "user-profile-path-slash": "user-profile-path",
                    "token-pat": "token", "email-escaped": "email"}
        for name, row in index.items():
            self.assertEqual(reasons[row], expected.get(name, name), name)
        self.assertEqual(staged["rows"][clean]["status"], "uploaded")
        self.assertEqual(staged["staged"], 1)
        text = (self.out / "report.json").read_text(encoding="utf-8")
        for secret in SECRETS:
            self.assertNotIn(secret, text.lower())
        self.assertEqual(sorted(p.name for p in self.out.rglob("*.png")), ["still-clean.png"])

    def test_rows_and_pixels_fail_closed(self):
        not_done = self.add("not-done", dump("Prepare"), status="failed: timeout")
        outside = self.add("outside", dump("Prepare"), allowlisted=False)
        # A crop has the 1 KiB floor, so the uniform frame reaches the pixel check.
        blank = self.add("blank", dump("Prepare"), kind="crop-probe", image="blank")
        tiny = self.add("tiny", dump("Prepare"), kind="page", image=None)
        busy_png(self.checkout / "docs/readme-assets/tiny.png", size=(20, 20))
        fake = self.add("fake", dump("Prepare"), image="text")
        missing = self.add("missing", dump("Prepare"), image=None)
        code, staged = self.run_check()
        self.assertEqual(code, 1, "nothing passed, so the job must fail")
        self.assertIsNone(staged, "nothing is staged when nothing passed")
        self.assertFalse(self.out.exists())
        # Rerun with one good row to read the reasons.
        good = self.add("good", dump("Prepare"))
        code, staged = self.run_check()
        self.assertEqual(code, 0)
        reasons = {r["row"]: r.get("reason") for r in staged["rows"]}
        self.assertEqual(reasons[not_done], "not-done")
        self.assertEqual(reasons[outside], "not-allowlisted")
        self.assertIsNone(staged["rows"][outside]["path"], "a path off the allowlist is not echoed")
        self.assertEqual(reasons[blank], "blank-image")
        self.assertEqual(reasons[tiny], "image-bytes")
        self.assertEqual(reasons[fake], "not-png")
        self.assertEqual(reasons[missing], "missing-image")
        self.assertEqual(staged["rows"][good]["status"], "uploaded")

    def test_unusable_inputs_stop_before_anything_is_staged(self):
        self.add("clean", dump("Prepare"))
        self.allowlist.append("docs/readme-assets/receipt.json")
        code, staged = self.run_check()
        self.assertEqual(code, 2, "an allowlist entry that is not a PNG path is refused")
        self.assertIsNone(staged)


if __name__ == "__main__":
    unittest.main()
