#!/usr/bin/env python3
"""Drive a verified installed Bambu Studio build on an isolated hidden desktop.

This deliberately records observations rather than treating a successful click
as proof of a feature. It requires the hosted Squirrel installation receipt,
uses only the cheap Lowlevel route, and writes private raw captures to a caller
owned directory. Do not upload that directory without separate privacy review.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
import time
from pathlib import Path

from recapture import App, Runner, cheap, find_control

SHA = re.compile(r"^[0-9a-f]{40}$")
HEX256 = re.compile(r"^[0-9a-f]{64}$")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_installation(receipt: dict, exe: Path, source: str, tag: str) -> None:
    if os.environ.get("GITHUB_ACTIONS") != "true" or os.environ.get("RUNNER_ENVIRONMENT") != "github-hosted":
        raise ValueError("The packaged GUI drive requires a disposable GitHub-hosted runner")
    if receipt.get("status") != "verified" or receipt.get("runner") != "github-hosted-windows":
        raise ValueError("The isolated Squirrel installation is not verified")
    if receipt.get("source_commit") != source or receipt.get("release_tag") != tag:
        raise ValueError("Installation receipt source or release differs from the requested build")
    expected = receipt.get("installed_exe_sha256", "")
    if not HEX256.fullmatch(expected) or sha256(exe) != expected:
        raise ValueError("Installed executable differs from the verified Squirrel package")
    version = receipt.get("package_version", "")
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("Installation receipt has no valid package version")
    expected_path = Path(os.environ["LOCALAPPDATA"]) / "BambuStudioMD3" / ("app-" + version) / "bambu-studio.exe"
    if exe.resolve() != expected_path.resolve():
        raise ValueError("Executable is not inside this runner's isolated Squirrel installation")


def visible_labels(records: list[dict]) -> list[str]:
    return sorted({str(r.get("label") or r.get("name")) for r in records
                   if r.get("kind") in ("window", "tool") and r.get("shown")
                   and r.get("on_screen") and (r.get("label") or r.get("name"))})


def visible(records: list[dict], label: str, owner: int | None = None) -> dict | None:
    return find_control(records, label, owner)


class Drive:
    def __init__(self, app: App, output: Path, source: str, tag: str, exe_hash: str, run_id: str):
        self.app = app
        self.runner = Runner(app, "en")
        self.output = output
        self.rows: list[dict] = []
        self.images: list[dict] = []
        self.identity = {"source_commit": source, "release_tag": tag, "hosted_run_id": run_id,
                         "installed_exe_sha256": exe_hash}

    def capture(self, label: str, hwnd: int) -> dict:
        name = f"{len(self.images):03d}-{re.sub('[^a-z0-9-]+', '-', label.lower()).strip('-')}.png"
        path = self.output / name
        self.app.shot(hwnd, str(path))
        record = {"file": name, "sha256": sha256(path), "bytes": path.stat().st_size,
                  "privacy": "restricted; visual review required before publication"}
        self.images.append(record)
        return record

    def observe(self, label: str, action, expected: tuple[str, ...], owner=None,
                *, timeout: float = 12, require_change: bool = False) -> bool:
        """Capture both sides and require visible probe evidence after the action."""
        row = {"name": label, "status": "unverified", "expected_visible": list(expected)}
        self.rows.append(row)
        try:
            before = self.app.probe()
            row["before_visible"] = visible_labels(before)
            row["before_image"] = self.capture(label + "-before", owner or self.app.main)
            t0 = time.monotonic()
            input_at = action()
            if not isinstance(input_at, float):
                input_at = t0
            row["action_ms"] = round((time.monotonic() - t0) * 1000)
            deadline = time.monotonic() + timeout
            after = []
            while time.monotonic() < deadline:
                after = self.app.probe()
                if all(visible(after, name, owner) for name in expected):
                    break
                time.sleep(0.4)
            row["ready_ms"] = round((time.monotonic() - t0) * 1000)
            row["input_to_ready_ms"] = round((time.monotonic() - input_at) * 1000)
            row["after_visible"] = visible_labels(after)
            first_hit = next((visible(after, name, owner) for name in expected
                              if visible(after, name, owner)), None)
            after_window = (first_hit or {}).get("top") or owner or self.app.main
            row["after_window"] = after_window
            row["after_image"] = self.capture(label + "-after", after_window)
            present = all(visible(after, name, owner) for name in expected)
            changed = row["before_visible"] != row["after_visible"]
            row["status"] = "probe_confirmed" if present and (changed or not require_change) else "unverified"
            row["visual_review"] = "pending"
            if not present:
                row["reason"] = "Expected controls were not visible in the post-action layout probe"
            elif require_change and not changed:
                row["reason"] = "The visible control set did not change after the action"
            return row["status"] == "probe_confirmed"
        except Exception as exc:
            row["status"] = "blocked"
            row["reason"] = f"{type(exc).__name__}: {exc}"
            return False

    def nav(self, label: str, expected: tuple[str, ...], timeout=12) -> bool:
        def action():
            records = self.app.probe()
            rail = next((r for r in records if r.get("kind") == "window"
                         and r.get("name") == "Navigation rail" and r.get("on_screen")), None)
            if not rail:
                raise RuntimeError("No on-screen navigation rail")
            target = visible(records, label, rail["hwnd"])
            if not target:
                raise RuntimeError(f"No navigation control labelled {label!r}")
            input_at = time.monotonic()
            self.runner.click_control(self.app.main, target)
            return input_at
        observed = self.observe("open-" + label.lower(), action, expected, self.app.main,
                                timeout=timeout, require_change=True)
        if not expected and observed:
            self.rows[-1]["status"] = "unverified"
            self.rows[-1]["reason"] = "Navigation changed visible controls, but no page-specific marker was established"
        return observed and bool(expected)

    def click(self, label: str, expected: tuple[str, ...], *, scope=None,
              timeout=12, require_change=True) -> bool:
        def action():
            records = self.app.probe()
            parent = next((r for r in records if r.get("kind") == "window"
                           and r.get("name") == scope and r.get("on_screen")), None) if scope else None
            if scope and not parent:
                raise RuntimeError(f"No on-screen scope named {scope!r}")
            target = visible(records, label, parent["hwnd"] if parent else self.app.main)
            if not target:
                raise RuntimeError(f"No on-screen control labelled {label!r}")
            input_at = time.monotonic()
            self.runner.click_control(target.get("top") or self.app.main, target)
            return input_at
        return self.observe("click-" + label.lower(), action, expected, self.app.main,
                            timeout=timeout, require_change=require_change)

    def unavailable(self, name: str, reason: str) -> None:
        self.rows.append({"name": name, "status": "unverified", "reason": reason})

    def run(self):
        self.observe("installed-shell", lambda: None, ("Home",), self.app.main)
        self.nav("Prepare", ("Ink", "Process", "Objects"), timeout=30)
        # The three tabs are scoped by the sidebar, never the main navigation rail.
        for tab, marker in (("Ink", "Search inks"), ("Process", "Search settings"),
                            ("Objects", "Search plate, object and part")):
            self.click(tab, (marker,), scope="Sidebar", timeout=10,
                       require_change=(tab != "Ink"))
        self.nav("Preview", ("Print plate",), timeout=30)
        self.unavailable("print-and-nozzle-send",
                         "Preview print affordance was inspected; no paired dual-nozzle printer is supplied and no send action was attempted")
        self.nav("Device", (), timeout=30)
        self.unavailable("lan-transfer-and-cancel",
                         "Device navigation was inspected; no paired LAN printer or transfer endpoint is supplied")
        self.unavailable("camera-autoplay",
                         "Device navigation was inspected; no live paired camera is supplied to establish streaming")
        self.nav("Project", ("Workspace",), timeout=30)
        self.click("Workspace", ("New workspace", "Open workspace", "Save workspace"), timeout=20)
        for tab, marker in (("Checklist", "Edit / due date / link"),
                            ("Calendar", "Add planned print"), ("Files", "Add project 3MF")):
            self.click(tab, (marker,), timeout=10)
        self.unavailable("workspace-save-reopen-history",
                         "Needs a deterministic project fixture and a private save path; no save/reopen was attempted")
        self.observe("open-model-creator", lambda: self.command_at("invoke Model Creator"),
                     ("Describe the model", "Provider executable path"), None,
                     timeout=30, require_change=True)
        self.unavailable("model-generation",
                         "Provider sign-in and renderer execution were not supplied; no generation or plate import was attempted")
        self.unavailable("localization-three-modes",
                         "The installed binary must be driven separately with persisted English, Cantonese, and bilingual settings")

    def command_at(self, text: str) -> float:
        input_at = time.monotonic()
        self.app.command(text)
        return input_at


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--exe", type=Path, required=True)
    ap.add_argument("--install-receipt", type=Path, required=True)
    ap.add_argument("--source-commit", required=True)
    ap.add_argument("--release-tag", required=True)
    ap.add_argument("--hosted-run-id", required=True)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    source = args.source_commit.lower()
    if not SHA.fullmatch(source) or not re.fullmatch(r"md3-v\d+", args.release_tag):
        ap.error("source commit or release tag is malformed")
    if not re.fullmatch(r"[0-9]{1,20}", args.hosted_run_id) or args.hosted_run_id != os.environ.get("GITHUB_RUN_ID"):
        ap.error("hosted run ID must match the current runner")
    if args.output.exists():
        ap.error("output directory already exists; this drive never overwrites prior evidence")
    receipt = json.loads(args.install_receipt.read_text(encoding="utf-8-sig"))
    validate_installation(receipt, args.exe, source, args.release_tag)
    runner_temp = os.environ.get("RUNNER_TEMP")
    if not runner_temp or not args.output.resolve().is_relative_to(Path(runner_temp).resolve()):
        ap.error("output must be a new child of the disposable runner's temporary directory")
    args.output.mkdir(parents=True)
    datadir = args.output / "profile"
    probe_dir = args.output / "probe"
    datadir.mkdir()
    probe_dir.mkdir()
    desktop = "bsbehavior-" + str(os.getpid())
    app = App(str(args.exe), str(datadir), desktop, str(probe_dir))
    drive = Drive(app, args.output, source, args.release_tag,
                  receipt["installed_exe_sha256"], args.hosted_run_id)
    cleanup_error = None
    try:
        app.start()
        frame = next((w for w in app.windows() if w["handle"] == app.main), {})
        drive.identity["window"] = {key: frame.get(key) for key in ("title", "class", "width", "height")}
        drive.identity["language"] = "English assumed from fresh profile; visible wording must be reviewed"
        drive.identity["theme"] = "not measured"
        drive.identity["display_scale"] = "not measured"
        drive.run()
    finally:
        try:
            if app.pid is not None:
                app.stop()
            else:
                # App.start may have created the named desktop before launch failed.
                cheap("close_headless_desktop", name=desktop)
        except Exception as exc:
            cleanup_error = f"{type(exc).__name__}: {exc}"
        report = {"schema": 1, **drive.identity, "package_version": receipt["package_version"],
                  "runner": "github-hosted-windows", "desktop": desktop, "rows": drive.rows,
                  "images": drive.images, "privacy": "restricted; inspect pixels and metadata before publication",
                  "cleanup": "verified" if cleanup_error is None else "failed: " + cleanup_error}
        (args.output / "behavior-report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 2 if cleanup_error or any(r["status"] == "blocked" for r in drive.rows) else 0


if __name__ == "__main__":
    sys.exit(main())
