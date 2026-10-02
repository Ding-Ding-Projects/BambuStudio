#!/usr/bin/env python3
"""Exercise installed native controls using the cheap hidden-desktop input route.

One bounded scope per invocation keeps evidence compatible with the existing
automation recipient and reader. No automation mutation or layout-probe command
is used. Native UI Automation is read-only. Raw evidence requires pixel review.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
import gettext
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time

from recapture import cheap

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("packaged_behavior", HERE / "drive-packaged-behavior.py")
behavior = importlib.util.module_from_spec(spec)
spec.loader.exec_module(behavior)


def require(value, message):
    if not value:
        raise RuntimeError(message)


def native_worker(request_path: Path, output: Path):
    """Run on the owned desktop, including the cheap CLI's keyboard targeting.

    UIA patterns are never invoked. In particular there is no ValuePattern setter,
    InvokePattern, menu command dispatch, WM_SETTEXT or application test command.
    """
    import comtypes.client
    from comtypes import COMError
    request = json.loads(request_path.read_text(encoding="utf-8"))
    user = ctypes.WinDLL("user32", use_last_error=True)
    user.SetProcessDpiAwarenessContext.argtypes = [wintypes.HANDLE]
    user.SetProcessDpiAwarenessContext(wintypes.HANDLE(-4))
    user.GetThreadDpiAwarenessContext.restype = wintypes.HANDLE
    user.GetAwarenessFromDpiAwarenessContext.argtypes = [wintypes.HANDLE]
    require(user.GetAwarenessFromDpiAwarenessContext(user.GetThreadDpiAwarenessContext()) == 2,
            "Native observer is not per-monitor DPI aware")
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.ScreenToClient.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.POINT)]
    user.SetWindowPos.argtypes = [wintypes.HWND, wintypes.HWND, ctypes.c_int, ctypes.c_int,
                                  ctypes.c_int, ctypes.c_int, wintypes.UINT]
    hwnd, pid = int(request["hwnd"]), int(request["pid"])
    actual = wintypes.DWORD()
    user.GetWindowThreadProcessId(hwnd, ctypes.byref(actual))
    require(actual.value == pid and pid > 0, "Native input target is not owned")
    operation = request["operation"]
    if operation in ("keys", "text"):
        class GUIThreadInfo(ctypes.Structure):
            _fields_ = [("cbSize", wintypes.DWORD), ("flags", wintypes.DWORD),
                ("hwndActive", wintypes.HWND), ("hwndFocus", wintypes.HWND),
                ("hwndCapture", wintypes.HWND), ("hwndMenuOwner", wintypes.HWND),
                ("hwndMoveSize", wintypes.HWND), ("hwndCaret", wintypes.HWND), ("rcCaret", wintypes.RECT)]
        info = GUIThreadInfo()
        info.cbSize = ctypes.sizeof(info)
        user.GetGUIThreadInfo.argtypes = [wintypes.DWORD, ctypes.POINTER(GUIThreadInfo)]
        thread = user.GetWindowThreadProcessId(hwnd, ctypes.byref(actual))
        require(user.GetGUIThreadInfo(thread, ctypes.byref(info)) and info.hwndFocus,
                "Native keyboard focus unavailable")
        hwnd = info.hwndFocus
        user.GetWindowThreadProcessId(hwnd, ctypes.byref(actual))
        require(actual.value == pid, "Keyboard focus left the owned process")
    if operation == "click":
        point = wintypes.POINT(*request["point"])
        require(user.ScreenToClient(hwnd, ctypes.byref(point)), "No client coordinate conversion")
        cheap("mouse_click", hwnd=hwnd, x=point.x, y=point.y, button=request.get("button", "left"))
    elif operation == "keys":
        cheap("win_send_keys", hwnd=hwnd, keys=request["keys"])
    elif operation == "text":
        cheap("type_text", hwnd=hwnd, text=request["text"])
    elif operation == "resize":
        width, height = request["size"]
        require(400 <= width <= 4000 and 300 <= height <= 4000, "Invalid native frame size")
        require(user.SetWindowPos(hwnd, None, 0, 0, width, height, 0x0002 | 0x0004 | 0x0010),
                "Native frame resize failed")
    elif operation != "observe":
        raise RuntimeError("Unsupported native operation")
    time.sleep(0.2)
    if operation != "observe":
        # A click can create or destroy a top level. The controller re-enumerates
        # owned windows before a separate observation, without replaying input.
        atomic_json(output, {"pid": pid, "rows": []})
        return
    comtypes.client.GetModule("UIAutomationCore.dll")
    from comtypes.gen.UIAutomationClient import CUIAutomation, IUIAutomation, IUIAutomationValuePattern
    automation = comtypes.client.CreateObject(CUIAutomation, interface=IUIAutomation)
    walker = automation.RawViewWalker
    rows = []
    # Every root was enumerated from the exact installation, profile and launch.
    for root_handle in request["roots"]:
        root_pid = wintypes.DWORD()
        user.GetWindowThreadProcessId(root_handle, ctypes.byref(root_pid))
        require(root_pid.value == pid, "Observation root ownership changed")
        root = automation.ElementFromHandle(root_handle)
        stack = [(root, 0, None)]
        while stack:
            element, depth, parent = stack.pop()
            require(len(rows) < 4000 and depth <= 32, "Native accessibility tree exceeds bound")
            try:
                if element.CurrentProcessId != pid:
                    continue
                rect = element.CurrentBoundingRectangle
                row = {"id": len(rows), "parent": parent, "top": root_handle,
                       "hwnd": int(element.CurrentNativeWindowHandle),
                       "name": str(element.CurrentName)[:512],
                       "automation_id": str(element.CurrentAutomationId)[:128],
                       "type": int(element.CurrentControlType),
                       "enabled": bool(element.CurrentIsEnabled),
                       "offscreen": bool(element.CurrentIsOffscreen),
                       "focused": bool(element.CurrentHasKeyboardFocus),
                       "rect": [rect.left, rect.top, rect.right, rect.bottom]}
                if row["type"] == 50004:
                    try:
                        pattern = element.GetCurrentPattern(10002).QueryInterface(IUIAutomationValuePattern)
                        row["value"] = str(pattern.CurrentValue)[:1024]
                    except COMError:
                        pass
                rows.append(row)
                children = []
                child = walker.GetFirstChildElement(element)
                while child:
                    require(len(children) < 1000, "Native child count exceeds bound")
                    children.append(child)
                    child = walker.GetNextSiblingElement(child)
                stack.extend((child, depth + 1, row["id"]) for child in reversed(children))
            except COMError:
                raise RuntimeError("Native accessibility tree changed during observation")
    atomic_json(output, {"pid": pid, "rows": rows})


def atomic_json(path, value):
    temporary = path.with_suffix(".pending")
    temporary.write_text(json.dumps(value), encoding="utf-8")
    os.replace(temporary, path)


class Driver:
    def __init__(self, args, app, scratch):
        self.args, self.app, self.scratch = args, app, scratch
        self.sequence, self.rows, self.images = 0, [], []
        self.native, self.probe = [], []
        self.translation = gettext.NullTranslations()
        if args.language == "yue_HK":
            catalog = args.exe.parent / "resources/i18n/yue_HK/BambuStudio.mo"
            require(catalog.is_file(), "Installed Cantonese catalog is missing")
            with catalog.open("rb") as stream:
                self.translation = gettext.GNUTranslations(stream)

    def label(self, english):
        return self.translation.gettext(english)

    def worker(self, operation="observe", hwnd=None, **values):
        windows = self.app.windows()
        require(any(w["handle"] == self.app.main for w in windows), "Owned main frame disappeared")
        self.sequence += 1
        request = self.scratch / f"input-{self.sequence}.json"
        output = self.scratch / f"observe-{self.sequence}.json"
        request.write_text(json.dumps({"pid": self.app.pid, "hwnd": hwnd or self.app.main,
            "roots": [w["handle"] for w in windows], "operation": operation, **values}), encoding="utf-8")
        command = subprocess.list2cmdline([sys.executable, str(Path(__file__).resolve()),
            "--native-request", str(request), "--native-output", str(output)])
        cheap("launch_on_headless_desktop", name=self.app.desktop, command=command)
        deadline = time.monotonic() + 25
        while time.monotonic() < deadline and not output.exists():
            time.sleep(0.1)
        require(output.exists(), "Owned native observation timed out")
        data = json.loads(output.read_text(encoding="utf-8"))
        require("error" not in data, "Native helper failed: " + str(data.get("error")))
        require(data.get("pid") == self.app.pid and isinstance(data.get("rows"), list),
                "Native observation identity mismatch")
        if operation != "observe":
            return self.worker()
        self.native = data["rows"]
        self.probe = self.app.probe()
        header = behavior.probe_header(self.probe, self.args.language, self.args.theme, self.args.scale)
        require(next(r for r in self.probe if r.get("kind") == "header").get("pid") == self.app.pid,
                "Layout observation identity mismatch")
        return header

    def candidates(self, name=None, kind=None, top=None):
        def matches(row):
            text = row["name"].replace("&", "").strip()
            text = text.split("\t")[0].rstrip(".\u2026 ")
            expected = self.label(name) if name is not None else None
            if expected is not None:
                expected = expected.rstrip(".\u2026 ")
            return (not row["offscreen"] and (kind is None or row["type"] == kind)
                    and (top is None or row["top"] == top)
                    and (expected is None or text == expected or text.startswith(expected + "\n")
                         or text.startswith(expected + " · ") or text.startswith(expected + "\t")))
        return [row for row in self.native if matches(row)]

    def one(self, name=None, kind=None, top=None):
        rows = self.candidates(name, kind, top)
        require(len(rows) == 1, "Native target missing or ambiguous: " + str(name))
        return rows[0]

    def filename_entry(self, top):
        # The hosted OS image is English even when the product locale changes.
        # Do not translate the operating system's common-dialog control names.
        rows = [r for r in self.native if not r["offscreen"] and r["top"] == top
                and r["type"] == 50004 and r["name"].replace("&", "").strip() == "File name:"]
        require(len(rows) == 1 and rows[0].get("value", "") == "",
                "Native filename field is unavailable, ambiguous, or not empty")
        return rows[0]

    def capture(self, label, hwnd=None, stable_source=None):
        require(len(self.images) < 30, "Scope exceeds encrypted capture inventory bound")
        handle = hwnd or self.app.main
        windows = self.app.windows()
        frame = next((w for w in windows if w["handle"] == handle), None)
        require(frame is not None, "Capture target is not an owned top-level window")
        require(frame.get("dpi") == round(96 * self.args.scale), "Actual window DPI differs from requested scale")
        name = f"{len(self.images):03d}-{label}.png"
        path = self.args.output / name
        if stable_source is None:
            result = cheap("screenshot", hwnd=handle, output_path=str(path))
        else:
            shutil.copyfile(stable_source, path)
            result = {"rendered_ok": True}
        require(result.get("rendered_ok") is True, "Native capture did not confirm rendering")
        from PIL import Image
        with Image.open(path) as image:
            image.verify()
        with Image.open(path) as image:
            require(image.width >= 80 and image.height >= 40 and
                    any(low != high for low, high in image.convert("RGB").getextrema()),
                    "Native capture is blank or too small")
            dimensions = [image.width, image.height]
        self.images.append({"file": name, "sha256": behavior.sha256(path),
            "bytes": path.stat().st_size, "captured_at_utc": datetime.now(timezone.utc).isoformat(),
            "pixels": dimensions, "native_dpi": frame.get("dpi"),
            "privacy": "restricted_pixel_review_pending"})
        return name

    def record(self, label, operation="observe", hwnd=None, capture_hwnd=None, **values):
        before_focus = [r["name"] for r in self.native if r["focused"]]
        started = time.monotonic()
        header = self.worker(operation, hwnd, **values)
        # No raw labels or paths are copied to the public receipt by the wrapper.
        overflow = [{key: r.get(key) for key in ("top", "name", "label", "rect", "client", "text_width",
                     "text_clipped", "hint_clipped", "clipped_by_parent", "starved", "zero_sized")}
                    for r in self.probe if r.get("kind") == "window" and r.get("on_screen") and
                    any(r.get(key) for key in ("text_clipped", "hint_clipped", "clipped_by_parent", "starved", "zero_sized"))]
        live_handles = {w["handle"] for w in self.app.windows()}
        focused_tops = {r["top"] for r in self.native if r["focused"] and r["top"] in live_handles}
        image_handle = next(iter(focused_tops)) if len(focused_tops) == 1 else (
            capture_hwnd if capture_hwnd in live_handles else self.app.main)
        row = {"operation": label, "input": operation, "status": "observed",
               "elapsed_ms": round((time.monotonic() - started) * 1000), "tuple": header,
               "before_focus": before_focus, "native": [r for r in self.native if not r["offscreen"]], "overflow": overflow,
               "capture": self.capture(label, image_handle)}
        self.rows.append(row)
        return row

    def click(self, label, target, button="left"):
        require(target["enabled"], "Native target is disabled")
        left, top, right, bottom = target["rect"]
        require(right > left and bottom > top, "Native target has no measurable area")
        return self.record(label, "click", hwnd=target["top"],
                           point=[(left + right) // 2, (top + bottom) // 2], button=button,
                           capture_hwnd=target["top"])

    def key(self, label, keys, top=None):
        return self.record(label, "keys", hwnd=top, keys=keys, capture_hwnd=top)

    def type(self, label, text, top):
        return self.record(label, "text", hwnd=top, text=text, capture_hwnd=top)

    def exact_client(self):
        requested = tuple(map(int, self.args.viewport.split("x")))
        for _ in range(4):
            self.worker()
            top = next(r for r in self.probe if r.get("kind") == "toplevel" and r.get("hwnd") == self.app.main)
            client = top["client"]
            if (client["w"], client["h"]) == requested:
                return
            frame = next(w for w in self.app.windows() if w["handle"] == self.app.main)
            self.worker("resize", size=[frame["width"] + requested[0] - client["w"],
                                         frame["height"] + requested[1] - client["h"]])
        raise RuntimeError("Actual client size did not reach the requested viewport")

    def menu_items(self, top):
        return [r for r in self.candidates(kind=50011, top=top)]

    def menu_checks(self, prefix, search, restored_top):
        top = search["top"]
        original = [r["name"] for r in self.menu_items(top)]
        require(original, "Menu has no native accessible items")
        self.click(prefix + "-search", search)
        self.key(prefix + "-tab", ["tab"], top)
        require(any(r["focused"] and r["name"] == self.label("Regex mode") for r in self.native),
                "Tab did not reach the native regex control")
        self.click(prefix + "-search-return", self.one("Search menu", kind=50004, top=top))
        require(any(r["focused"] and r["type"] == 50004 and r["top"] == top for r in self.native),
                "Search entry did not recover native keyboard focus")
        self.type(prefix + "-literal-match", original[0].split("\t")[0].split(" · ")[0], top)
        require(self.menu_items(top) and len(self.menu_items(top)) <= len(original),
                "Literal search did not preserve a matching item")
        self.key(prefix + "-literal-clear", ["esc"], top)
        self.type(prefix + "-no-match", "zz_fixture_no_match_927", top)
        require(not self.menu_items(top) and self.candidates("No matches.", top=top),
                "No-match state was not exposed by the actual menu")
        self.key(prefix + "-clear-escape", ["esc"], top)
        require([r["name"] for r in self.menu_items(top)] == original,
                "First Escape did not restore the menu items")
        self.click(prefix + "-regex", self.one("Regex mode", top=top))
        search = self.one("Search menu", kind=50004, top=top)
        self.click(prefix + "-regex-search", search)
        self.type(prefix + "-regex-match", "^.*$", top)
        require([r["name"] for r in self.menu_items(top)] == original,
                "Regex match changed the complete menu inventory")
        self.key(prefix + "-regex-clear", ["esc"], top)
        self.key(prefix + "-dismiss", ["esc"], top)
        require(not self.candidates("Search menu", kind=50004, top=top), "Second Escape did not dismiss menu")
        require(any(r["focused"] and r["top"] == restored_top for r in self.native),
                "Menu dismissal did not restore keyboard focus to its invoking surface")
        require(not any(row for step in self.rows if step["operation"].startswith(prefix + "-")
                        for row in step["overflow"] if row["top"] == top),
                "Menu controls overflow their measured layout")

    def menus(self):
        self.click("prepare", self.one("Prepare"))
        # An empty scene's context menu is the short root menu. No model is added.
        canvases = [r for r in self.probe if r.get("on_screen") and "GLCanvas" in r.get("class", "")]
        require(canvases, "Prepare has no observed scene canvas")
        canvas = max(canvases, key=lambda r: r["screen"]["w"] * r["screen"]["h"])
        rect = canvas["screen"]
        self.record("open-context", "click", point=[rect["x"] + rect["w"] // 2,
                    rect["y"] + rect["h"] // 2], button="right")
        search = self.one("Search menu", kind=50004)
        require(1 <= len(self.menu_items(search["top"])) <= 5,
                "Short context menu fixture did not expose one to five rows")
        self.menu_checks("root", search, self.app.main)
        self.record("reopen-context", "click", point=[rect["x"] + rect["w"] // 2,
                    rect["y"] + rect["h"] // 2], button="right")
        root_search = self.one("Search menu", kind=50004)
        self.click("open-nested", self.one("Add Primitive", kind=50011))
        searches = [r for r in self.candidates("Search menu", kind=50004) if r["top"] != root_search["top"]]
        require(len(searches) == 1, "Nested menu search did not appear")
        self.menu_checks("nested", searches[0], root_search["top"])
        require(self.candidates("Search menu", kind=50004, top=root_search["top"]),
                "Nested Escape dismissed the parent menu")
        self.key("root-final-dismiss", ["esc"], root_search["top"])

    def open_vocabulary(self, prefix):
        self.click(prefix + "-edit", self.one("Edit"))
        self.click(prefix + "-preferences", self.one("Preferences", kind=50011))
        search = self.one("Search settings", kind=50004)
        self.click(prefix + "-search", search)
        self.type(prefix + "-find-wording", self.label("Personal vocabulary"), search["top"])
        return search["top"]

    def title_pixels(self, label):
        """Observe stable real pixels while requiring original native text.

        Equality/change is evidence of display persistence, not OCR or proof of
        the exact painted replacement. Full raw images remain review-required.
        """
        from PIL import Image
        last = None
        scratch_image = self.scratch / (label + "-stability.png")
        for attempt in range(6):
            self.worker()
            title = self.one("Personal vocabulary", kind=50020)
            require(title["name"] == self.label("Personal vocabulary"),
                    "Native title no longer exposes original wording")
            require(not self.candidates("Fixture wording alpha") and not self.candidates("Fixture wording beta"),
                    "Replacement wording leaked into native accessibility")
            top = next(r for r in self.probe if r.get("kind") == "toplevel" and r.get("hwnd") == title["top"])
            origin = top["rect"]
            rect = [title["rect"][0] - origin["x"], title["rect"][1] - origin["y"],
                    title["rect"][2] - origin["x"], title["rect"][3] - origin["y"]]
            rendered = cheap("screenshot", hwnd=title["top"], output_path=str(scratch_image))
            require(rendered.get("rendered_ok") is True, "Title stability capture did not render")
            captured_at = datetime.now(timezone.utc).isoformat()
            with Image.open(scratch_image) as image:
                require(0 <= rect[0] < rect[2] <= image.width and 0 <= rect[1] < rect[3] <= image.height,
                        "Observed title leaves its captured surface")
                crop = image.crop(rect).convert("RGB")
                signature = hashlib.sha256(str(crop.size).encode("ascii") + crop.tobytes()).hexdigest()
            if last == signature:
                name = self.capture(label + "-stable-title", title["top"], scratch_image)
                self.images[-1]["captured_at_utc"] = captured_at
                self.rows[-1]["display_title"] = {"capture": name, "crop": rect,
                    "pixel_sha256": signature, "stable_samples": 2, "attempts": attempt + 1,
                    "native_original_text": True, "exact_painted_text_review": "pending"}
                return signature
            last = signature
            time.sleep(0.25)
        raise RuntimeError("Title pixels did not stabilize within six bounded observations")

    def upload_vocabulary(self, prefix, target, fixture, preferences):
        self.click(prefix + "-load", target)
        dialogs = [w for w in self.app.windows() if w["class"] == "#32770" and w["handle"] != preferences]
        require(len(dialogs) == 1, "File picker is missing or ambiguous")
        dialog = dialogs[0]["handle"]
        self.click(prefix + "-filename-focus", self.filename_entry(dialog))
        require(any(r["focused"] and r["type"] == 50004 and r["top"] == dialog for r in self.native),
                "File picker filename edit did not receive focus")
        self.type(prefix + "-filename", str(fixture), dialog)
        self.key(prefix + "-filename-submit", ["enter"], dialog)

    def restart(self, label):
        previous = self.app
        old_pid = previous.pid
        previous.stop()
        require(previous.owned_teardown_verified and previous.desktop_closed_verified,
                "Prior native instance teardown is unverified")
        probe = self.scratch / (label + "-probe")
        probe.mkdir()
        self.app = behavior.HostedApp(previous.exe, previous.datadir,
                                      "bsnative-" + str(os.getpid()) + "-" + label, str(probe))
        self.app.holder_lifetime = 1800
        # Reuse the same isolated profile and existing local display cache. Never
        # seed, copy or reconstruct a vocabulary file between these processes.
        self.app.start(timeout=240)
        require(self.app.pid != old_pid, "Fresh process identity was not observed")
        self.exact_client()
        row = self.record(label)
        row["restart"] = {"previous_pid": old_pid, "new_pid": self.app.pid,
                          "previous_teardown_verified": True,
                          "launch_started_utc": str(self.app.launch_started)}

    def vocabulary(self):
        preferences = self.open_vocabulary("initial")
        target = self.one("Load JSON")
        baseline = self.title_pixels("original")
        previous = baseline
        # Native text must stay original. Only genuine captured pixels are used
        # for change/preservation checks; exact replacement text needs review.
        source = self.label("Personal vocabulary")
        for index, replacement in enumerate(("Fixture wording alpha", "Fixture wording beta")):
            fixture = self.scratch / f"neutral-{index}.json"
            fixture.write_text(json.dumps({"schemaVersion": 1, "entries": {source: replacement}}), encoding="utf-8")
            self.upload_vocabulary(f"valid-{index}", target, fixture, preferences)
            current = self.title_pixels(f"valid-{index}")
            require(current != previous and current != baseline, "Title pixels did not change for the replacement")
            previous = current
            target = self.one("Replace JSON")
        invalid = self.scratch / "neutral-invalid.json"
        invalid.write_text(json.dumps({"schemaVersion": 2, "entries": {source: "Invalid replacement"}}), encoding="utf-8")
        self.upload_vocabulary("invalid", target, invalid, preferences)
        require(self.candidates("The vocabulary file could not be applied. Use valid version 1 JSON within the supported size limits.")
                and self.candidates("Replace JSON")
                and not self.candidates("Invalid replacement"),
                "Invalid JSON did not visibly preserve the active mapping")
        require(self.title_pixels("invalid") == previous, "Invalid JSON changed the painted title")
        self.click("clear-wording", self.one("Clear personal vocabulary"))
        require(self.candidates("Personal vocabulary") and self.candidates("Load JSON") and
                not self.candidates("Fixture wording beta"), "Clear did not restore native original wording")
        require(self.title_pixels("cleared") == baseline, "Clear did not restore original title pixels")

    def vocabulary_persistence(self):
        preferences = self.open_vocabulary("initial")
        baseline = self.title_pixels("original")
        fixture = self.scratch / "neutral-persistence.json"
        fixture.write_text(json.dumps({"schemaVersion": 1,
            "entries": {self.label("Personal vocabulary"): "Fixture wording beta"}}), encoding="utf-8")
        self.upload_vocabulary("valid", self.one("Load JSON"), fixture, preferences)
        mapped = self.title_pixels("loaded")
        require(mapped != baseline and self.candidates("Replace JSON"), "Title replacement did not become active")
        self.restart("restart-loaded")
        self.open_vocabulary("restored")
        require(self.candidates("Replace JSON") and self.title_pixels("restored") == mapped,
                "Mapped title pixels did not survive a fresh native process")
        self.click("clear-wording", self.one("Clear personal vocabulary"))
        require(self.candidates("Load JSON") and self.title_pixels("cleared") == baseline,
                "Clear did not restore original title pixels")
        self.restart("restart-cleared")
        self.open_vocabulary("cleared")
        require(self.candidates("Personal vocabulary") and self.candidates("Load JSON")
                and self.candidates("Original wording is active.") and self.title_pixels("restarted-clear") == baseline,
                "Clear did not persist across a fresh native process")

    def slice_controls(self):
        self.click("prepare", self.one("Prepare"))
        state = behavior.model_snapshot(self.probe, pid=self.app.pid,
            profile_tag=Path(self.app.datadir).name, main_hwnd=self.app.main)
        require(state is not None and state["object_count"] == 0, "Empty project preflight was not observed")
        controls = [self.one(label) for label in ("Slice and Print", "Slice and Send")]
        require(all(not row["enabled"] for row in controls),
                "Empty project unexpectedly enables a combined action; no action was sent")
        # Disabled-control click preflight cannot send anything to a device. Never
        # add a model, select a printer, or submit a device confirmation here.
        for index, row in enumerate(controls):
            before = {w["handle"] for w in self.app.windows()}
            left, top, right, bottom = row["rect"]
            require(right > left and bottom > top, "Combined action has no measurable area")
            self.record(f"disabled-action-{index}", "click", hwnd=row["top"],
                        point=[(left + right) // 2, (top + bottom) // 2])
            require(not self.one(("Slice and Print", "Slice and Send")[index])["enabled"],
                    "Disabled combined action changed state")
            require({w["handle"] for w in self.app.windows()} == before, "Disabled action opened a dialog")
        for label in ("Slice options", "Print options"):
            self.one(label)
            glyphs = [r for r in self.probe if r.get("on_screen") and r.get("name") == self.label(label)]
            require(len(glyphs) == 1 and glyphs[0].get("label") == "\u25be", "Options control has no visible chevron")
        relevant = [r for r in self.probe if r.get("on_screen") and
                    any(self.label(label) in str(r.get("label", "")) for label in
                        ("Slice and Print", "Slice and Send", "Slice plate", "Print plate"))]
        require(len(relevant) >= 2 and not any(r.get(key) for r in relevant for key in
                ("text_clipped", "clipped_by_parent", "starved", "zero_sized")),
                "Combined action layout overflows")

    def inspect(self, operation):
        require(operation in ("project_inspect", "presets_list"), "Only read-only native commands are permitted")
        self.app.windows()  # Refresh exact process ownership immediately before attaching.
        result = subprocess.run([str(self.args.cli), "command", operation, "--json", "--workspace",
            str(self.scratch), "--instance", str(self.app.pid)], capture_output=True, text=True,
            timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
        require(len(result.stdout) <= 1048576 and result.returncode == 0, "Native read-only observation failed")
        data = json.loads(result.stdout)
        require(data.get("ok") is True, "Native read-only observation was rejected")
        return data["result"]

    def combined(self, action):
        self.click("prepare", self.one("Prepare"))
        presets = self.inspect("presets_list")
        require(all(presets.get(key) for key in ("printer", "print", "filament")),
                "Required bundled presets unavailable; combined action remains unverified")
        before = self.inspect("project_inspect")
        require(before.get("objects") == [], "Combined action fixture requires an empty project")
        fixture = self.scratch / "cube.stl"
        shutil.copyfile(HERE.parents[1] / "tests/automation-fixtures/cube.stl", fixture)
        self.click("open-file", self.one("File"))
        self.click("open-import", self.one("Import", kind=50011))
        self.click("import-cube", self.one("Import 3MF/STL/STEP/SVG/OBJ/AMF", kind=50011))
        dialogs = [w for w in self.app.windows() if w["class"] == "#32770"]
        require(len(dialogs) == 1, "Model import picker is missing or ambiguous")
        picker = dialogs[0]["handle"]
        self.click("import-filename-focus", self.filename_entry(picker))
        require(any(r["focused"] and r["type"] == 50004 and r["top"] == picker for r in self.native),
                "Import filename edit did not receive focus")
        self.type("import-filename", str(fixture), picker)
        self.key("import-submit", ["enter"], picker)
        imported = self.inspect("project_inspect")
        require(len(imported.get("objects", [])) == 1 and imported.get("currentPlate") == 0,
                "Native import did not produce the requested cube on plate zero")
        require(imported.get("plates") and imported["plates"][0].get("sliceReady") is False,
                "Fixture unexpectedly has reusable slice output")
        self.rows[-1]["fixture_sha256"] = behavior.sha256(fixture)
        caption = "Slice and Print" if action == "print" else "Slice and Send"
        title = "Send print job" if action == "print" else "Send to Printer storage"
        self.click("combined-start", self.one(caption))
        deadline = time.monotonic() + 300
        observations, confirmation = [], None
        while time.monotonic() < deadline:
            state = self.inspect("project_inspect")
            observations.append({"slicing": state.get("slicing"), "plates": state.get("plates"),
                                 "currentPlate": state.get("currentPlate")})
            self.worker()
            confirmation = next((w for w in self.app.windows() if w["class"] == "#32770"
                                 and self.label(title) in w.get("title", "")), None)
            if confirmation:
                break
            time.sleep(1)
        self.rows[-1]["native_slice_observations"] = observations
        require(confirmation is not None, "Expected device confirmation unavailable after slicing; printer/account or preflight state requires review")
        state = self.inspect("project_inspect")
        require(state.get("slicing") is False and state.get("currentPlate") == 0 and
                state.get("plates") and state["plates"][0].get("sliceReady") is True,
                "Device confirmation opened without ready output for the requested plate")
        self.record("combined-confirmation", capture_hwnd=confirmation["handle"])
        # No Enter or submit click is sent to a printer dialog. Escape may only
        # dismiss it; a dialog that ignores Escape produces an unverified result.
        self.key("combined-confirmation-cancel", ["esc"], confirmation["handle"])
        require(not any(w["handle"] == confirmation["handle"] and w.get("visible", True)
                        for w in self.app.windows()), "Device confirmation did not dismiss with Escape")

    def combined_print(self):
        self.combined("print")

    def combined_send(self):
        self.combined("send")

    def cancellation(self):
        # The current public read-only observation exposes slicing/sliceReady,
        # but no generation identity or pending continuation. This scope must
        # not report stale-event cancellation as verified from an idle snapshot.
        self.rows.append({"operation": "explicit-cancel-and-stale-generation", "status": "unverified",
            "reason": "Requires an observable in-flight generation, an accessible cancel target, and a newer-generation completion fixture"})
        raise RuntimeError("Cancellation and stale-generation native interaction remain unverified")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("exe", "cli", "install-receipt", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--release-tag", required=True)
    parser.add_argument("--scope", choices=("menus", "vocabulary", "vocabulary-persistence", "slice-controls", "combined-print", "combined-send", "cancellation"), required=True)
    parser.add_argument("--language", choices=behavior.MODES, default="en")
    parser.add_argument("--theme", choices=("light", "dark"), default="light")
    parser.add_argument("--scale", type=float, choices=(1.0, 1.25, 1.5, 2.0), default=1.0)
    parser.add_argument("--viewport", choices=("1200x800", "1000x600"), default="1200x800")
    args = parser.parse_args()
    require(os.environ.get("GITHUB_ACTIONS") == "true" and os.environ.get("RUNNER_ENVIRONMENT") == "github-hosted"
            and os.environ.get("RUNNER_OS") == "Windows", "Only disposable hosted Windows execution is authorized")
    require(behavior.SHA.fullmatch(args.source_commit) and re.fullmatch(r"md3-v\d+", args.release_tag), "Malformed source identity")
    install = json.loads(args.install_receipt.read_text(encoding="utf-8-sig"))
    behavior.validate_installation(install, args.exe, args.source_commit, args.release_tag)
    root = Path(os.environ["RUNNER_TEMP"]).resolve()
    require(args.output.resolve().is_relative_to(root), "Evidence output escapes temporary root")
    scratch = Path(tempfile.mkdtemp(prefix="native-interface-owned-", dir=root))
    profile, probe = scratch / "profile", scratch / "probe"
    profile.mkdir()
    probe.mkdir()
    behavior.seed_profile(profile, args.language, args.theme)
    os.environ["BAMBU_AUTOMATION"] = "1"
    os.environ["BAMBU_AUTOMATION_ROOTS"] = str(scratch)
    app = behavior.HostedApp(str(args.exe), str(profile), "bsnative-" + str(os.getpid()), str(probe))
    app.holder_lifetime = 1800
    drive = None
    status, failure, teardown = "failed", None, False
    try:
        drive = Driver(args, app, scratch)
        app.start(timeout=240)
        drive.exact_client()
        drive.record("native-ready")
        getattr(drive, args.scope.replace("-", "_"))()
        status = "runtime_verified"
    except Exception as exc:
        failure = f"{type(exc).__name__}: {exc}"
    finally:
        try:
            final_app = drive.app if drive else app
            final_app.stop()
            teardown = bool(final_app.owned_teardown_verified and final_app.desktop_closed_verified)
        except Exception as exc:
            failure = failure or f"{type(exc).__name__}: {exc}"
        if not teardown:
            status = "failed"
        report = {"schema": 1, "status": status, "source_commit": args.source_commit,
            "release_tag": args.release_tag, "run_id": os.environ["GITHUB_RUN_ID"],
            "exe_sha256": behavior.sha256(args.exe), "cli_sha256": behavior.sha256(args.cli),
            "install_receipt_sha256": behavior.sha256(args.install_receipt), "scope": args.scope,
            "package_asset_sha256": install.get("asset_sha256"),
            "driver_sha256": behavior.sha256(Path(__file__)),
            "requested_tuple": {"language": args.language, "theme": args.theme,
                "scale": args.scale, "viewport": args.viewport},
            "operations": drive.rows if drive else [], "captures": drive.images if drive else [],
            "capture_method": "lowlevel-computer-use-cheap hidden desktop",
            "privacy": "restricted_pixel_review_pending", "hardware": "unverified_no_printer_commands",
            "teardown_verified": teardown, "failure": failure}
        (args.output / "runtime.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return 0 if status == "runtime_verified" else 1


if __name__ == "__main__":
    if len(sys.argv) == 5 and sys.argv[1] == "--native-request" and sys.argv[3] == "--native-output":
        watchdog = threading.Timer(22, lambda: os._exit(3))
        watchdog.daemon = True
        watchdog.start()
        try:
            native_worker(Path(sys.argv[2]), Path(sys.argv[4]))
        except Exception as exc:
            atomic_json(Path(sys.argv[4]), {"error": type(exc).__name__})
        finally:
            watchdog.cancel()
    else:
        raise SystemExit(main())
