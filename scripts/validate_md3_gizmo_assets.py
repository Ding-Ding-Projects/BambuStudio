#!/usr/bin/env python3
"""Validate the MD3-tokenized BambuStudio gizmo/chrome SVG completion set."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import tempfile
from pathlib import Path
from xml.etree import ElementTree as ET

EXPECTED = [
    "toolbar_tooltip.svg", "toolbar_tooltip_hover.svg",
    "fit_camera.svg", "fit_camera_hover.svg", "fit_camera_dark.svg", "fit_camera_dark_hover.svg",
    "text_B.svg", "text_B_dark.svg", "text_T.svg", "text_T_dark.svg",
]
for axis in "xyz":
    for position in ("min", "center", "max"):
        EXPECTED += [f"align_{axis}_{position}.svg", f"align_{axis}_{position}_dark.svg"]
    EXPECTED += [f"distribute_{axis}.svg", f"distribute_{axis}_dark.svg"]
EXPECTED = sorted(EXPECTED)

DIMS = {
    "toolbar_tooltip.svg": (30, 22), "toolbar_tooltip_hover.svg": (30, 22),
    "fit_camera.svg": (33, 32), "fit_camera_hover.svg": (33, 32),
    "fit_camera_dark.svg": (33, 32), "fit_camera_dark_hover.svg": (33, 32),
    "text_B.svg": (20, 20), "text_B_dark.svg": (20, 20),
    "text_T.svg": (20, 20), "text_T_dark.svg": (20, 20),
}
for name in EXPECTED:
    if name.startswith(("align_", "distribute_")):
        DIMS[name] = (36, 36)

AXIS = {"x": "#EA4335", "y": "#34A853", "z": "#4C8BF5"}
ALLOWED_COLORS = {
    # Light semantic roles.
    "#F4F2F9", "#E8E7EE", "#44464E", "#1A1B1F", "#C5C6D0", "#75777F",
    # Dark/inverse semantic roles.
    "#25262B", "#2F3036", "#393A41", "#CDCED8", "#F1F0F7", "#94959F", "#4A4C54",
    # Functional 3D viewport data colors from the checked-in design contract.
    *AXIS.values(),
}
FORBIDDEN_LEGACY_COLORS = {"#00AE42", "#808080", "#6B6B6B", "#F4F4F4"}
ALLOWED_TAGS = {"svg", "rect", "path", "circle", "polygon"}
FORBIDDEN_TEXT = ("<script", "<style", "<text", "<image", "<use", "foreignObject", "url(", "currentColor", "var(")
HEX_RE = re.compile(r"#[0-9A-Fa-f]{6}")


def tag_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def detect_root(explicit: Path | None) -> Path:
    if explicit is not None:
        return explicit.resolve()
    # Works both in the kit overlay and after copying to a repository's scripts/ directory.
    return Path(__file__).resolve().parents[1]


def render_with_inkscape(path: Path, out: Path) -> None:
    inkscape = shutil.which("inkscape")
    if not inkscape:
        raise RuntimeError("--render requested but Inkscape is not on PATH")
    subprocess.run(
        [inkscape, str(path), "--export-type=png", f"--export-filename={out}", "--export-width=144", "--export-height=144"],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
    )
    if not out.exists() or out.stat().st_size < 100:
        raise RuntimeError(f"renderer produced no usable PNG for {path.name}")


def validate(root: Path, render: bool) -> dict[str, object]:
    images = root / "resources" / "images"
    errors: list[str] = []
    warnings: list[str] = []
    details: list[dict[str, object]] = []

    found = sorted(p.name for p in images.glob("*.svg") if p.name in EXPECTED)
    missing = sorted(set(EXPECTED) - set(found))
    if missing:
        errors.append("missing expected assets: " + ", ".join(missing))

    render_dir_obj = tempfile.TemporaryDirectory(prefix="bambustudio-md3-svg-") if render else None
    render_dir = Path(render_dir_obj.name) if render_dir_obj else None

    hashes: dict[str, str] = {}
    try:
        for name in EXPECTED:
            path = images / name
            if not path.exists():
                continue
            raw = path.read_text(encoding="utf-8")
            lowered = raw.lower()
            for marker in FORBIDDEN_TEXT:
                if marker.lower() in lowered:
                    errors.append(f"{name}: forbidden SVG feature {marker!r}")
            try:
                tree = ET.parse(path)
            except ET.ParseError as exc:
                errors.append(f"{name}: XML parse failed: {exc}")
                continue
            root_el = tree.getroot()
            if tag_name(root_el.tag) != "svg":
                errors.append(f"{name}: root element is not svg")
                continue
            width, height = DIMS[name]
            expected_viewbox = f"0 0 {width} {height}"
            if root_el.attrib.get("width") != str(width) or root_el.attrib.get("height") != str(height):
                errors.append(f"{name}: expected width/height {width}x{height}")
            if root_el.attrib.get("viewBox") != expected_viewbox:
                errors.append(f"{name}: expected viewBox {expected_viewbox!r}")
            for element in tree.iter():
                local = tag_name(element.tag)
                if local not in ALLOWED_TAGS:
                    errors.append(f"{name}: unsupported SVG element <{local}>")
                for key, value in element.attrib.items():
                    if key.lower().endswith("href") or "javascript:" in value.lower():
                        errors.append(f"{name}: external or executable reference in {key}")
            colors = {c.upper() for c in HEX_RE.findall(raw)}
            unknown = colors - ALLOWED_COLORS
            if unknown:
                errors.append(f"{name}: non-contract colors: {', '.join(sorted(unknown))}")
            legacy = colors & FORBIDDEN_LEGACY_COLORS
            if legacy:
                errors.append(f"{name}: legacy colors remain: {', '.join(sorted(legacy))}")
            if name.startswith(("align_", "distribute_")):
                match = re.match(r"(?:align|distribute)_([xyz])", name)
                axis = match.group(1) if match else ""
                if not axis or AXIS[axis] not in colors:
                    errors.append(f"{name}: missing design-contract {axis.upper()} axis color {AXIS[axis]}")
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            hashes[name] = digest
            if render and render_dir is not None:
                try:
                    render_with_inkscape(path, render_dir / f"{name}.png")
                except Exception as exc:  # Report all assets rather than stopping at the first.
                    errors.append(f"{name}: render failed: {exc}")
            details.append({"file": name, "sha256": digest, "colors": sorted(colors), "bytes": path.stat().st_size})
    finally:
        if render_dir_obj:
            render_dir_obj.cleanup()

    # Each semantic tile must remain distinct; accidental copy/paste would collapse an operation.
    semantic = [n for n in EXPECTED if n.startswith(("align_", "distribute_"))]
    reverse: dict[str, list[str]] = {}
    for name in semantic:
        if name in hashes:
            reverse.setdefault(hashes[name], []).append(name)
    for duplicate_group in reverse.values():
        if len(duplicate_group) > 1:
            errors.append("duplicate semantic artwork: " + ", ".join(duplicate_group))

    manager = root / "src" / "slic3r" / "GUI" / "Gizmos" / "GLGizmosManager.cpp"
    if manager.exists():
        source = manager.read_text(encoding="utf-8", errors="replace")
        unreferenced = [name for name in EXPECTED if f'/images/{name}' not in source]
        if unreferenced:
            warnings.append("current manager does not reference: " + ", ".join(unreferenced))

    parity = root / "docs" / "features" / "design-system" / "md3-parity-register.md"
    parity_status = "not-present"
    if parity.exists():
        text = parity.read_text(encoding="utf-8", errors="replace")
        for line in text.splitlines():
            if line.startswith("| gizmo-rail-svg-icons |"):
                parity_status = line.rsplit("|", 2)[-2].strip()
                break
        if "done" not in parity_status.lower():
            warnings.append("gizmo-rail-svg-icons remains non-done in the parity register; update only after native light/dark runtime evidence")

    return {
        "ok": not errors,
        "root": str(root),
        "asset_directory": str(images),
        "expected_assets": len(EXPECTED),
        "validated_assets": len(details),
        "rendered": render,
        "parity_status": parity_status,
        "errors": errors,
        "warnings": warnings,
        "details": details,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, help="repository or overlay root")
    parser.add_argument("--render", action="store_true", help="also rasterize every asset with Inkscape")
    parser.add_argument("--json", type=Path, help="write the complete report as JSON")
    args = parser.parse_args()
    report = validate(detect_root(args.root), args.render)
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"MD3 gizmo asset validation: {'PASS' if report['ok'] else 'FAIL'}")
    print(f"assets: {report['validated_assets']}/{report['expected_assets']} | rendered: {report['rendered']}")
    for warning in report["warnings"]:
        print(f"WARNING: {warning}")
    for error in report["errors"]:
        print(f"ERROR: {error}")
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
