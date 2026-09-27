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
import shutil
import sys
import tempfile
import time
import zipfile
from datetime import datetime, timezone
from pathlib import Path
from PIL import Image

from recapture import App, Runner, cheap, find_control
from hosted_process import owned_processes, process_snapshot

SHA = re.compile(r"^[0-9a-f]{40}$")
HEX256 = re.compile(r"^[0-9a-f]{64}$")
MODES = ("en", "yue_HK", "bilingual_en_yue_HK")


class HostedApp(App):
    """Bind a window only to this launch, installation, profile and desktop."""

    def __init__(self, exe, datadir, desktop, probe_dir):
        super().__init__(exe, datadir, desktop, probe_dir)
        self.launch_started = None
        self.launch_pid = None
        self.adopted_pids = []

    def _desktop_windows(self):
        return cheap("list_headless_windows", name=self.desktop)["windows"]

    def windows(self):
        windows = self._desktop_windows()
        candidates = owned_processes(
            process_snapshot(), exe=self.exe, datadir=self.datadir,
            launched_at=self.launch_started, launch_pid=self.launch_pid,
            desktop_pids={int(w["process_id"]) for w in windows},
        )
        self.adopted_pids = candidates
        if self.pid not in candidates:
            self.pid = candidates[0] if candidates else None
        return [w for w in windows if int(w["process_id"]) in candidates]

    def start(self, timeout=240):
        os.environ["BAMBU_LAYOUT_PROBE"] = "1"
        os.environ["BAMBU_LAYOUT_PROBE_TAG"] = os.path.basename(self.datadir)
        cheap("create_headless_desktop", name=self.desktop)
        self.launch_started = datetime.now(timezone.utc)
        self.launch_pid = cheap("launch_on_headless_desktop", name=self.desktop,
                                command=f'"{self.exe}" --datadir "{self.datadir}"')["pid"]
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            frame = self.find(lambda w: w["class"] == "wxWindowNR"
                              and w["width"] >= 1000 and w["height"] >= 600)
            if frame:
                self.main = frame["handle"]
                self.pid = int(frame["process_id"])
                time.sleep(8)
                return
            time.sleep(1)
        raise RuntimeError("No owned main frame appeared on the named hidden desktop")

    def stop(self):
        # Re-check live identity before terminating anything. An exited launch PID
        # is normal in a relaunch and must never obscure the drive's first error.
        errors = []
        try:
            self.windows()
            for pid in self.adopted_pids:
                try:
                    cheap("kill_process", pid=pid, force=True)
                except Exception as exc:
                    errors.append(f"owned PID {pid}: {type(exc).__name__}: {exc}")
        except Exception as exc:
            errors.append(f"ownership recheck: {type(exc).__name__}: {exc}")
        try:
            cheap("close_headless_desktop", name=self.desktop)
        except Exception as exc:
            errors.append(f"desktop closure: {type(exc).__name__}: {exc}")
        if errors:
            raise RuntimeError("; ".join(errors))
MODE_LABELS = {
    "en": {"prepare": "Prepare", "ink": "Ink", "process": "Process", "objects": "Objects",
           "ink_search": "Search filaments", "settings": "Search settings", "preview": "Preview", "project": "Project"},
    "yue_HK": {"prepare": "準備", "ink": "耗材", "process": "打印設定", "objects": "物件",
               "ink_search": "搵墨水", "settings": "搜尋設定", "preview": "預覽", "project": "項目"},
    "bilingual_en_yue_HK": {"prepare": "Prepare", "ink": "Ink", "process": "Process",
                             "objects": "Objects", "ink_search": "Search filaments", "settings": "Search settings",
                             "preview": "Preview", "project": "Project"},
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def preserve_logs(datadir: Path, output: Path) -> list[dict]:
    """Copy only bounded original app logs into the restricted output inventory."""
    log_dir = datadir / "log"
    if not log_dir.is_dir():
        return []
    records = []
    destination = output / "restricted-logs"
    for source in sorted(log_dir.glob("*.log"))[:8]:
        if (not re.fullmatch(r"[A-Za-z0-9_.-]{1,100}", source.name)
                or source.is_symlink() or not source.is_file()
                or source.stat().st_size > 10_000_000):
            continue
        destination.mkdir(exist_ok=True)
        target = destination / source.name
        shutil.copyfile(source, target)
        records.append({"file": target.name,
                        "sha256": sha256(target), "bytes": target.stat().st_size,
                        "privacy": "restricted original log; do not print or publish"})
    return records


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


def seed_profile(datadir: Path, mode: str, theme: str = "light") -> None:
    """Create only a disposable config, using AppConfig's JSON and checksum format."""
    if mode not in MODES or theme not in ("light", "dark") or any(datadir.iterdir()):
        raise ValueError("Language profile is not new or the mode is unsupported")
    body = json.dumps({"app": {"language": mode, "dark_color_mode": "1" if theme == "dark" else "0"}},
                      ensure_ascii=False, indent=4)
    digest = hashlib.md5(body.encode("utf-8")).hexdigest().upper()
    (datadir / "BambuStudio.conf").write_bytes(
        (body + "\n# MD5 checksum " + digest + "\n").encode("utf-8"))


def probe_header(records: list[dict], mode: str, theme: str | None = None,
                 scale: float | None = None) -> dict:
    header = next((r for r in records if r.get("kind") == "header"), None)
    if header is None or header.get("language") != mode:
        raise RuntimeError(f"Layout probe did not confirm requested language {mode}")
    if not isinstance(header.get("dpi_scale"), (int, float)) or header["dpi_scale"] <= 0:
        raise RuntimeError("Layout probe did not report a valid display scale")
    if theme is not None and (not isinstance(header.get("dark"), bool)
                              or header["dark"] != (theme == "dark")):
        raise RuntimeError("Layout probe theme differs from requested theme")
    if scale is not None and abs(float(header["dpi_scale"]) - scale) > 0.02:
        raise RuntimeError("Measured native display scale differs from requested scale")
    return {key: header.get(key) for key in ("language", "dpi_scale", "dark", "density")}


class Drive:
    def __init__(self, app: App, output: Path, source: str, tag: str, exe_hash: str, run_id: str,
                 mode: str, verification_commit: str = "", theme: str = "light",
                 scale: float = 1.0, viewport: tuple[int, int] = (1200, 800)):
        self.app = app
        self.runner = Runner(app, "en")
        self.output = output
        self.rows: list[dict] = []
        self.images: list[dict] = []
        self.mode = mode
        self.theme = theme
        self.scale = scale
        self.viewport = viewport
        self.labels = MODE_LABELS[mode]
        self.identity = {"source_commit": source, "release_tag": tag, "hosted_run_id": run_id,
                         "verification_commit": verification_commit,
                         "installed_exe_sha256": exe_hash,
                         "requested_tuple": {"language": mode, "theme": theme,
                                             "scale": scale, "viewport": list(viewport)}}

    def checked_header(self, records: list[dict]) -> dict:
        return probe_header(records, self.mode, self.theme, self.scale)

    def capture(self, label: str, hwnd: int) -> dict:
        name = f"{len(self.images):03d}-{re.sub('[^a-z0-9-]+', '-', label.lower()).strip('-')}.png"
        path = self.output / name
        result = cheap("screenshot", hwnd=hwnd, output_path=str(path))
        if result.get("rendered_ok") is not True:
            raise RuntimeError(f"PrintWindow did not confirm a rendered image for {label}")
        if not path.is_file() or path.stat().st_size < 2000:
            raise RuntimeError(f"No substantial screenshot file was written for {label}")
        with Image.open(path) as captured:
            captured.verify()
        with Image.open(path) as captured:
            rgb = captured.convert("RGB")
            if rgb.width < 200 or rgb.height < 150:
                raise RuntimeError(f"Captured frame dimensions are too small for {label}")
            extrema = rgb.getextrema()
            if all(low == high for low, high in extrema):
                raise RuntimeError(f"Captured frame is uniform for {label}")
        record = {"file": name, "sha256": sha256(path), "bytes": path.stat().st_size,
                  "captured_at_utc": datetime.now(timezone.utc).isoformat(),
                  "privacy": "restricted; visual review required before publication"}
        self.images.append(record)
        return record

    def observe(self, label: str, action, expected: tuple[str, ...], owner=None,
                *, timeout: float = 12, require_change: bool = False,
                require_new_marker: bool = False, require_enabled: bool = False) -> bool:
        """Capture both sides and require visible probe evidence after the action."""
        row = {"name": label, "status": "unverified", "expected_visible": list(expected)}
        self.rows.append(row)
        try:
            before = self.app.probe()
            row["before_header"] = self.checked_header(before)
            row["before_visible"] = visible_labels(before)
            marker_was_visible = any(visible(before, name, owner) for name in expected)
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
                self.checked_header(after)
                if all(visible(after, name, owner) for name in expected) and (
                    not require_enabled or all(visible(after, name, owner).get("enabled") for name in expected)
                ):
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
            enabled = not require_enabled or all(visible(after, name, owner).get("enabled")
                                                for name in expected if visible(after, name, owner))
            changed = row["before_visible"] != row["after_visible"]
            row["status"] = "probe_confirmed" if (present and enabled and
                (changed or not require_change) and (not require_new_marker or not marker_was_visible)) else "unverified"
            row["visual_review"] = "pending"
            if not present:
                row["reason"] = "Expected controls were not visible in the post-action layout probe"
            elif not enabled:
                row["reason"] = "Expected post-action control remained disabled"
            elif require_new_marker and marker_was_visible:
                row["reason"] = "The expected marker was already visible before the action"
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
              timeout=12, require_change=True, require_enabled=False) -> bool:
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
                            timeout=timeout, require_change=require_change,
                            require_enabled=require_enabled)

    def unavailable(self, name: str, reason: str) -> None:
        self.rows.append({"name": name, "status": "unavailable", "reason": reason})

    def unverified(self, name: str, reason: str) -> None:
        self.rows.append({"name": name, "status": "unverified", "reason": reason})

    def optional_nav(self, label: str) -> None:
        records = self.app.probe()
        rail = next((r for r in records if r.get("kind") == "window"
                     and r.get("name") == "Navigation rail" and r.get("on_screen")), None)
        if not rail or not visible(records, label, rail["hwnd"]):
            self.unavailable("open-" + label.lower(), "The navigation control is not present in this installed build")
            return
        self.nav(label, (), timeout=15)

    def dialog_text(self, name: str, button: str, dialog_title: str, value: str,
                    postcondition) -> bool:
        """Use a discovered native edit control, then inspect an independent result."""
        row = {"name": name, "status": "unverified", "action": button,
               "dialog_title": dialog_title}
        self.rows.append(row)
        try:
            before = self.app.probe()
            self.checked_header(before)
            row["before_visible"] = visible_labels(before)
            row["before_image"] = self.capture(name + "-before", self.app.main)
            target = visible(before, button, self.app.main)
            if not target:
                raise RuntimeError(f"No on-screen button labelled {button!r}")
            old_dialogs = {w["handle"] for w in self.app.windows() if w["class"] == "#32770"}
            input_at = time.monotonic()
            self.runner.click_control(target.get("top") or self.app.main, target)
            dialog = self.app.wait(lambda w: w["class"] == "#32770"
                                   and w["handle"] not in old_dialogs
                                   and dialog_title.lower() in w["title"].lower(),
                                   12, dialog_title)
            row["dialog_image"] = self.capture(name + "-dialog", dialog["handle"])
            children = cheap("list_child_windows", hwnd=dialog["handle"])["children"]
            edits = [c for c in children if c["class"].lower() == "edit" and c.get("visible")]
            if not edits:
                raise RuntimeError("The dialog exposes no visible native edit control")
            # In a file picker the filename edit is below its search field.
            edit = max(edits, key=lambda c: c.get("top", 0))
            cheap("win_set_control_text", hwnd=edit["handle"], text=value)
            cheap("win_send_keys", hwnd=dialog["handle"], keys=["enter"])
            deadline = time.monotonic() + 4
            while time.monotonic() < deadline:
                if all(w["handle"] != dialog["handle"] for w in self.app.windows()):
                    break
                time.sleep(0.4)
            else:
                buttons = [c for c in cheap("list_child_windows", hwnd=dialog["handle"])["children"]
                           if c["class"].lower() == "button" and c.get("visible")
                           and c.get("text", "").replace("&", "").strip().lower() in ("ok", "save", "open")]
                if len(buttons) != 1:
                    raise RuntimeError("The dialog stayed open and has no unique submit button")
                submit = buttons[0]
                cheap("mouse_click", hwnd=dialog["handle"],
                      x=submit["left"] + submit["width"] // 2,
                      y=submit["top"] + submit["height"] // 2)
                deadline = time.monotonic() + 12
                while time.monotonic() < deadline:
                    if all(w["handle"] != dialog["handle"] for w in self.app.windows()):
                        break
                    time.sleep(0.4)
                else:
                    raise RuntimeError("The dialog remained open after its submit button was clicked")
            row["input_to_ready_ms"] = round((time.monotonic() - input_at) * 1000)
            row["after_image"] = self.capture(name + "-after", self.app.main)
            after = self.app.probe()
            row["after_visible"] = visible_labels(after)
            evidence = postcondition(after)
            if evidence is None:
                raise RuntimeError("The expected result did not appear after dialog submission")
            row["result"] = evidence
            row["status"] = "probe_confirmed"
            row["visual_review"] = "pending"
            return True
        except Exception as exc:
            row["status"] = "blocked"
            row["reason"] = f"{type(exc).__name__}: {exc}"
            return False

    def workspace_roundtrip(self) -> None:
        title = "Hosted verification workspace"
        output_file = self.output / "fixture.bambu-workspace"
        self.click("Overview", ("Rename workspace",), timeout=10)
        renamed = self.dialog_text(
            "rename-workspace", "Rename workspace", "Workspace title", title,
            lambda records: {"title": title} if visible(records, title, self.app.main) else None)
        if not renamed:
            self.unavailable("workspace-save-reopen", "Workspace rename did not establish a distinct source state")
            return
        saved = self.dialog_text(
            "save-workspace", "Save workspace", "Save workspace", str(output_file),
            lambda _records: self.workspace_file_evidence(output_file))
        if not saved:
            self.unavailable("workspace-reopen", "No verified workspace file was saved")
            return
        self.click("New workspace", ("Rename workspace",), timeout=10)
        if visible(self.app.probe(), title, self.app.main):
            self.unavailable("workspace-reopen", "New workspace did not remove the prior title")
            return
        self.dialog_text(
            "reopen-workspace", "Open workspace", "Open workspace", str(output_file),
            lambda records: {"title": title, "file_sha256": sha256(output_file)}
            if visible(records, title, self.app.main) else None)

    def open_project_file(self, path: Path) -> bool:
        """Exercise File > Open Project using a checked-in, non-private 3MF."""
        row = {"name": "file-menu-open-project", "status": "unverified",
               "fixture_sha256": sha256(path) if path.is_file() else None,
               "action": "File > Open Project"}
        self.rows.append(row)
        if not path.is_file():
            row["status"] = "blocked"
            row["reason"] = "The checked-in 3MF fixture is absent"
            return False
        try:
            before = self.app.probe()
            row["before_header"] = self.checked_header(before)
            row["before_visible"] = visible_labels(before)
            row["before_image"] = self.capture("file-open-before", self.app.main)
            old_dialogs = {w["handle"] for w in self.app.windows() if w["class"] == "#32770"}
            start = time.monotonic()
            self.app.command("invoke Open Project")
            dialog = self.app.wait(lambda w: w["class"] == "#32770"
                                   and w["handle"] not in old_dialogs
                                   and w["width"] >= 400, 20, "Open Project dialog")
            row["dialog_image"] = self.capture("file-open-dialog", dialog["handle"])
            children = cheap("list_child_windows", hwnd=dialog["handle"])["children"]
            edits = [c for c in children if c["class"].lower() == "edit" and c.get("visible")]
            if not edits:
                raise RuntimeError("Open Project dialog has no visible native file edit")
            edit = max(edits, key=lambda c: c.get("top", 0))
            cheap("win_set_control_text", hwnd=edit["handle"], text=str(path))
            cheap("win_send_keys", hwnd=dialog["handle"], keys=["enter"])
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline:
                if all(w["handle"] != dialog["handle"] for w in self.app.windows()):
                    break
                time.sleep(0.5)
            else:
                raise RuntimeError("Open Project dialog did not close after file submission")
            after = self.app.probe()
            row["after_header"] = self.checked_header(after)
            row["after_visible"] = visible_labels(after)
            row["after_image"] = self.capture("file-open-after", self.app.main)
            row["input_to_ready_ms"] = round((time.monotonic() - start) * 1000)
            marker = path.stem
            row["status"] = "probe_confirmed" if any(marker.lower() in label.lower()
                                                         for label in row["after_visible"]) else "unverified"
            if row["status"] == "unverified":
                row["reason"] = "The frame survived File > Open, but the probe exposed no fixture-specific loaded marker"
            return row["status"] == "probe_confirmed"
        except Exception as exc:
            row["status"] = "blocked"
            row["reason"] = f"{type(exc).__name__}: {exc}"
            return False

    @staticmethod
    def workspace_file_evidence(path: Path) -> dict | None:
        if not path.is_file() or path.stat().st_size < 100:
            return None
        with zipfile.ZipFile(path) as bundle:
            names = set(bundle.namelist())
            if "Metadata/workspace.json" not in names:
                return None
            manifest = json.loads(bundle.read("Metadata/workspace.json"))
            if manifest.get("title") != "Hosted verification workspace":
                return None
        return {"file_sha256": sha256(path), "manifest_title": manifest["title"]}

    def prepare_tabs(self) -> None:
        l = self.labels
        self.nav(l["prepare"], (l["ink"], l["process"], l["objects"]), timeout=30)
        for name, marker in ((l["ink"], l["ink_search"]), (l["process"], l["settings"]),
                             (l["objects"], "Search plate, object and part.")):
            self.click(name, (marker,), scope="Sidebar", timeout=10,
                       require_change=(name != l["ink"]))

    def narrow_prepare(self) -> None:
        row = {"name": "narrow-prepare-tabs", "status": "unverified",
               "requested_size": [1000, 600]}
        self.rows.append(row)
        try:
            self.app.command(f"resize {self.app.main} 1000 600")
            time.sleep(1)
            records = self.app.probe()
            row["header"] = self.checked_header(records)
            frame = next((w for w in self.app.windows() if w["handle"] == self.app.main), None)
            if not frame or frame["width"] > 1020 or frame["height"] > 620:
                raise RuntimeError("Main frame did not resize to the narrow client area")
            row["actual_size"] = [frame["width"], frame["height"]]
            sidebar = next((r for r in records if r.get("kind") == "window"
                            and r.get("name") == "Sidebar" and r.get("on_screen")), None)
            if not sidebar:
                raise RuntimeError("Sidebar is not on screen after narrow resize")
            tabs = []
            for name in (self.labels["ink"], self.labels["process"], self.labels["objects"]):
                tab = visible(records, name, sidebar["hwnd"])
                if not tab:
                    raise RuntimeError(f"Prepare tab {name!r} is not on screen at narrow width")
                if any(tab.get(flag) for flag in ("text_clipped", "clipped_by_parent", "starved", "zero_sized")):
                    raise RuntimeError(f"Prepare tab {name!r} has a layout defect at narrow width")
                tabs.append({"label": name, "screen": tab["screen"]})
            row["tabs"] = tabs
            row["image"] = self.capture("narrow-prepare-tabs", self.app.main)
            row["status"] = "probe_confirmed"
            row["visual_review"] = "pending"
        except Exception as exc:
            row["status"] = "blocked"
            row["reason"] = f"{type(exc).__name__}: {exc}"

    def run_localized(self) -> None:
        self.prepare_tabs()
        self.narrow_prepare()
        self.unavailable("localized-print-workspace-device-flows",
                         "This language tuple drove Prepare only; the remaining flows require their own localized action ledger")

    def run(self):
        self.observe("installed-shell", lambda: None, ("Home",), self.app.main)
        self.rows[-1]["status"] = "capture_only"
        self.rows[-1]["reason"] = "A generic shell label does not prove a feature"
        project_fixture = Path(__file__).resolve().parents[2] / "resources" / "calib" / "filament_flow" / "flowrate-test-pass1.3mf"
        if not self.open_project_file(project_fixture):
            self.unverified("recent-project-tile", "A fixture-specific File > Open result was not established, so the recent tile was not clicked")
            return
        self.unverified("recent-project-tile", "The recent tile is inside a webview and has no verified cheap-route target yet")
        self.prepare_tabs()
        self.narrow_prepare()
        self.app.command(f"resize {self.app.main} 1200 800")
        fixture = Path(__file__).resolve().parents[2] / ".claude" / "skills" / "run-bambustudio" / "cube.stl"
        if fixture.is_file():
            loaded = self.observe("load-print-fixture", lambda: self.command_at(f"load {fixture}"),
                                  ("cube",), self.app.main, timeout=30, require_change=True,
                                  require_new_marker=True)
            if loaded:
                self.click("Slice plate", ("Print plate",), timeout=120, require_enabled=True)
            else:
                self.unavailable("slice-print-fixture", "The cube load was not confirmed in the layout probe")
        else:
            self.unavailable("load-print-fixture", "The checked-in cube STL fixture is unavailable")
        self.nav("Preview", ("Print plate",), timeout=30)
        self.unavailable("print-and-nozzle-send",
                         "Preview print affordance was inspected; no paired dual-nozzle printer is supplied and no send action was attempted")
        self.nav("Device", (), timeout=30)
        device_records = self.app.probe()
        device_controls = [label for label in visible_labels(device_records)
                           if any(part in label.lower() for part in ("camera", "liveview", "live view", "lan", "nozzle"))]
        device_image = self.capture("device-unpaired-state", self.app.main)
        self.rows.append({"name": "device-no-hardware-control-inventory", "status": "unavailable",
                          "candidate_controls": device_controls, "image": device_image,
                          "source_control": "Play or stop the camera live view",
                          "camera_control_visible": any("play or stop the camera live view" in label.lower()
                                                        for label in device_controls),
                          "reason": "The no-printer message is custom drawn; visible controls and pixels require review and do not prove a paired printer, transfer, or video stream"})
        self.optional_nav("Multi-device")
        self.unavailable("lan-transfer-and-cancel",
                         "Unpaired Device state and optional Multi-device navigation were inspected; pairing, transfer and cancellation need a LAN printer")
        self.unavailable("camera-autoplay",
                         "Camera control exposure was inspected without clicking Play; autoplay and streaming need a paired camera")
        self.nav("Project", ("Workspace",), timeout=30)
        self.click("Workspace", ("New workspace", "Open workspace", "Save workspace"), timeout=20)
        for tab, marker in (("Checklist", "Edit / due date / link"),
                            ("Calendar", "Add planned print"), ("Files", "Add project 3MF")):
            self.click(tab, (marker,), timeout=10)
        self.workspace_roundtrip()
        self.unavailable("project-portable-history",
                         "Project version history requires a saved 3MF member; the isolated workspace round trip does not prove embedded history")
        self.observe("open-model-creator", lambda: self.command_at("invoke Model Creator"),
                     ("Describe the model", "Provider executable path"), None,
                     timeout=30, require_change=True)
        self.unavailable("model-generation",
                         "Provider sign-in and renderer execution were not supplied; no generation or plate import was attempted")
        self.unavailable("localization-three-modes",
                         "See the separate en, yue_HK, and bilingual_en_yue_HK profile reports")

    def command_at(self, text: str) -> float:
        input_at = time.monotonic()
        self.app.command(text)
        return input_at


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--exe", type=Path, required=True)
    ap.add_argument("--install-receipt", type=Path, required=True)
    ap.add_argument("--source-commit", required=True)
    ap.add_argument("--verification-commit", required=True)
    ap.add_argument("--release-tag", required=True)
    ap.add_argument("--hosted-run-id", required=True)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--language", choices=MODES, default="en")
    ap.add_argument("--theme", choices=("light", "dark"), default="light")
    ap.add_argument("--scale", type=float, default=1.0)
    ap.add_argument("--viewport", choices=("1200x800", "1000x600"), default="1200x800")
    ap.add_argument("--scope", choices=("diagnostic", "behavior", "layout"), default="behavior")
    args = ap.parse_args()
    source = args.source_commit.lower()
    verifier = args.verification_commit.lower()
    if not SHA.fullmatch(source) or not SHA.fullmatch(verifier) or not re.fullmatch(r"md3-v\d+", args.release_tag):
        ap.error("source commit, verification commit or release tag is malformed")
    if args.scale not in (1.0, 1.25, 1.5, 2.0):
        ap.error("scale must be one of 1.0, 1.25, 1.5, 2.0")
    if args.scope == "behavior" and (args.language, args.theme, args.scale, args.viewport) != (
            "en", "light", 1.0, "1200x800"):
        ap.error("the full behavior ledger is bound to the English light 100% 1200x800 baseline")
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
    scratch = Path(tempfile.mkdtemp(prefix="bsbehavior-private-", dir=runner_temp))
    datadir = scratch / "profile"
    probe_dir = scratch / "probe"
    datadir.mkdir()
    probe_dir.mkdir()
    seed_profile(datadir, args.language, args.theme)
    desktop = "bsbehavior-" + str(os.getpid())
    app = HostedApp(str(args.exe), str(datadir), desktop, str(probe_dir))
    requested_size = tuple(int(part) for part in args.viewport.split("x"))
    drive = Drive(app, args.output, source, args.release_tag,
                  receipt["installed_exe_sha256"], args.hosted_run_id, args.language,
                  verifier, args.theme, args.scale, requested_size)
    cleanup_error = None
    try:
        app.start()
        frame = next((w for w in app.windows() if w["handle"] == app.main), {})
        drive.identity["window"] = {key: frame.get(key) for key in ("title", "class", "width", "height")}
        drive.identity["process"] = {"initial_pid": app.launch_pid,
                                     "selected_pid": app.pid,
                                     "relaunched": app.pid != app.launch_pid}
        if requested_size != (1200, 800):
            app.command(f"resize {app.main} {requested_size[0]} {requested_size[1]}")
            time.sleep(1)
        frame = next((w for w in app.windows() if w["handle"] == app.main), {})
        if abs(frame.get("width", 0) - requested_size[0]) > 20 or abs(frame.get("height", 0) - requested_size[1]) > 20:
            raise RuntimeError("Measured frame differs from requested viewport")
        expected_dpi = round(96 * args.scale)
        if frame.get("dpi") != expected_dpi:
            raise RuntimeError("Native window DPI differs from requested scale")
        drive.identity["measured_tuple"] = {**drive.checked_header(app.probe()),
                                             "viewport": [frame["width"], frame["height"]],
                                             "native_dpi": frame["dpi"]}
        if args.scope == "diagnostic":
            image = drive.capture("installed-shell-diagnostic", app.main)
            drive.rows.append({"name": "installed-shell-diagnostic", "status": "capture_only",
                               "image": image, "reason": "Launch and a rendered frame do not prove behavior"})
        elif args.scope == "layout":
            drive.run_localized()
        else:
            drive.run()
    except Exception as exc:
        drive.rows.append({"name": "launch-or-drive", "status": "blocked",
                           "reason": f"{type(exc).__name__}: {exc}"})
    finally:
        try:
            app.stop()
        except Exception as exc:
            cleanup_error = f"{type(exc).__name__}: {exc}"
    try:
        logs = preserve_logs(datadir, args.output)
    except Exception as exc:
        logs = []
        drive.rows.append({"name": "restricted-log-preservation", "status": "blocked",
                           "reason": f"{type(exc).__name__}: {exc}"})
    failed_rows = [r["name"] for r in drive.rows if r["status"] in ("blocked", "unverified")]
    verdict = "blocked" if cleanup_error or failed_rows else ("diagnostic_only" if args.scope == "diagnostic" else "pending_visual_review")
    report = {"schema": 2, **drive.identity, "scope": args.scope,
              "package_version": receipt["package_version"], "runner": "github-hosted-windows",
              "desktop": desktop,
              "rows": drive.rows, "images": drive.images, "restricted_logs": logs,
              "failed_rows": failed_rows,
              "privacy": "restricted; inspect pixels and metadata before publication",
              "cleanup": "verified" if cleanup_error is None else "failed: " + cleanup_error,
              "verdict": verdict}
    (args.output / "behavior-report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 2 if verdict == "blocked" else 0


if __name__ == "__main__":
    sys.exit(main())
