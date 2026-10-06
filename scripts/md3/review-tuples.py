#!/usr/bin/env python3
"""Offline requested-tuple inventory and native observation comparison, never acceptance."""
from __future__ import annotations

import argparse
import importlib.util
import itertools
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
AXES = {
    "language": ("en", "yue_HK", "bilingual_en_yue_HK"),
    "theme": ("light", "dark"),
    "scalePercent": (100, 125, 150, 200),
    "density": ("comfortable", "compact"),
    "motion": ("full", "reduced"),
    "geometry": ("normal", "minimum"),
}
BOUNDARIES = (
    "shared-control-callers", "shell-nested-fit", "specialized-continuations",
    "dense-settings-subforms", "device-nested-details", "reader-detail-variants",
    "workspace-inherited-details", "embedded-alternate-flows", "renderer-tool-interiors",
)


def positive(value, name):
    try:
        valid = type(value) in (int, float) and math.isfinite(value) and value > 0
    except OverflowError:
        valid = False
    if not valid:
        raise ValueError(name + " must be a finite positive number")
    return value


def minimum_geometry(measured_em):
    """Calculate native units, not DIP, from an independently measured native em."""
    positive(measured_em, "measured_em")
    width, height = 76 * measured_em, 49 * measured_em
    positive(width, "minimum width")
    positive(height, "minimum height")
    return {"w": max(1000, width), "h": max(600, height)}


def validate_requested(requested):
    if not isinstance(requested, dict) or set(requested) != set(AXES) | {"boundary"}:
        raise ValueError("Requested tuple must contain exactly the seven inventory fields")
    if requested["boundary"] not in BOUNDARIES:
        raise ValueError("Unknown review boundary")
    for key, values in AXES.items():
        if type(requested[key]) is not type(values[0]) or requested[key] not in values:
            raise ValueError("Unsupported requested " + key)


def inventory():
    """Fail closed if the reviewed queue changes instead of silently expanding scope."""
    scopes = json.loads((ROOT / "design/workflow-refresh/implementation-scopes.json").read_text(encoding="utf-8"))
    if tuple(row["id"] for row in scopes["remainingVisualCoverage"]) != BOUNDARIES:
        raise ValueError("Review boundary inventory changed; review this helper before use")
    return [{"requested": {"boundary": boundary, **dict(zip(AXES, values))},
             "status": "pending", "observed": None}
            for boundary in BOUNDARIES for values in itertools.product(*AXES.values())]


def local_validator():
    spec = importlib.util.spec_from_file_location("local_native_review", HERE / "local-native-review.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.validate_probe


def compare(requested, records, *, pid, hwnd, tag):
    """Compare existing native NDJSON fields. Never turn a native dump into a v1 receipt.

    Unsupported measurements stay unavailable. Even a matching dump is not source,
    privacy, interaction, visual-layout or full-tuple acceptance evidence.
    """
    validate_requested(requested)
    for name, value in (("pid", pid), ("hwnd", hwnd)):
        if type(value) is not int or value <= 0:
            raise ValueError(name + " must be a positive integer")
    if not isinstance(tag, str) or not tag.strip():
        raise ValueError("Expected probe tag is required")
    result = {"requested": dict(requested), "observed": {}, "status": "pending",
              "mismatches": [], "unavailable": ["effectiveMotion", "measuredNativeEm", "minimumGeometry"],
              "probeValidation": "pending", "acceptance": False}
    if records is None:
        result["unavailable"] += ["nativeProbe"]
        return result
    if (not isinstance(records, list) or not records or any(not isinstance(row, dict) for row in records)
            or records[-1] != {"kind": "end"}
            or sum(row.get("kind") == "end" for row in records) != 1):
        raise ValueError("Native probe is incomplete or ambiguous")
    headers = [row for row in records if row.get("kind") == "header"]
    frames = [row for row in records if row.get("kind") == "toplevel" and row.get("hwnd") == hwnd]
    if len(headers) != 1 or len(frames) != 1:
        raise ValueError("Native probe header or target is ambiguous")
    header, frame = headers[0], frames[0]
    if type(header.get("pid")) is not int or header["pid"] != pid or header.get("tag") != tag:
        raise ValueError("Native probe ownership mismatch")
    if type(frame.get("hwnd")) is not int or frame.get("shown") is not True:
        raise ValueError("Native probe target is not shown")
    scale = positive(header.get("dpi_scale"), "dpi_scale")
    if header.get("language") not in AXES["language"] or header.get("density") not in AXES["density"]:
        raise ValueError("Native probe language or density is unavailable")
    if type(header.get("dark")) is not bool:
        raise ValueError("Native probe theme is unavailable")
    client = frame.get("client")
    if not isinstance(client, dict) or any(type(client.get(key)) is not int or client[key] <= 0 for key in ("w", "h")):
        raise ValueError("Invalid measured client geometry")
    observed = {"language": header["language"], "theme": "dark" if header["dark"] else "light",
                "density": header["density"], "scalePercent": scale * 100,
                "clientNative": {key: client[key] for key in ("w", "h")}}
    positive(observed["scalePercent"], "scalePercent")
    result["observed"] = observed
    for key in ("language", "theme", "density", "scalePercent"):
        if observed[key] != requested[key]:
            result["mismatches"].append(key)
    # Pass independently requested values; never normalize the observed header.
    if not any(key in result["mismatches"] for key in ("language", "theme", "density")):
        local_validator()(records, pid, hwnd, tag,
                          expected_language=requested["language"],
                          expected_theme=requested["theme"],
                          expected_density=requested["density"])
        result["probeValidation"] = "native-validator-passed"
    else:
        result["probeValidation"] = "tuple-mismatch"
    if requested["geometry"] == "normal":
        # Target is DIP. Native integer allocation permits only pixel rounding.
        expected = {"w": 1200 * scale, "h": 800 * scale}
        if any(abs(client[key] - expected[key]) > 0.5 for key in expected):
            result["mismatches"].append("normalGeometry")
    if result["mismatches"]:
        result["status"] = "mismatch"
    return result


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate JSON key")
        result[key] = value
    return result


def parse_json(text):
    def invalid_constant(_):
        raise ValueError("Non-finite JSON number")
    return json.loads(text, object_pairs_hook=unique_object, parse_constant=invalid_constant)


def read_text(path):
    if path.stat().st_size > 16 * 1024 * 1024:
        raise ValueError("Input exceeds 16 MiB")
    return path.read_text(encoding="utf-8-sig")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("inventory")
    comparison = commands.add_parser("compare")
    comparison.add_argument("--requested", type=Path, required=True)
    comparison.add_argument("--probe", type=Path)
    comparison.add_argument("--pid", type=int, required=True)
    comparison.add_argument("--hwnd", type=int, required=True)
    comparison.add_argument("--tag", required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "inventory":
            output = {"kind": "requested-review-inventory", "acceptance": False, "tuples": inventory()}
        else:
            records = [parse_json(line) for line in read_text(args.probe).splitlines() if line.strip()] if args.probe else None
            output = compare(parse_json(read_text(args.requested)), records,
                             pid=args.pid, hwnd=args.hwnd, tag=args.tag)
        print(json.dumps(output, ensure_ascii=False, allow_nan=False, indent=2))
        return 0 if args.command == "inventory" else 2 if output["status"] == "mismatch" else 3
    except (ValueError, OSError, KeyError, TypeError, OverflowError) as error:
        print(json.dumps({"status": "invalid", "acceptance": False, "reason": str(error)}))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
