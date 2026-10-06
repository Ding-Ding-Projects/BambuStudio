#!/usr/bin/env python3
"""Validate an offline, private native interaction ledger. Never drive the UI."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import importlib.util
import json
import math
from pathlib import Path
import sys
import tempfile

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("native_review", HERE / "local-native-review.py")
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)
require = native.require
JSON_LIMIT = 1024 * 1024
PROBE_LIMIT = 16 * 1024 * 1024


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate JSON key")
        result[key] = value
    return result


def finite_float(value):
    result = float(value)
    require(math.isfinite(result), "Nonfinite JSON number")
    return result


def reject_constant(value):
    raise ValueError("Nonfinite JSON constant")


def strict_json(value):
    return json.loads(value, object_pairs_hook=unique_object,
                      parse_float=finite_float, parse_constant=reject_constant)


def bounded_read(path, limit):
    native.regular(path)
    require(type(limit) is int and 0 < limit <= PROBE_LIMIT, "Invalid JSON read bound")
    # The read itself is bounded. A prior stat cannot constrain a file that
    # grows or is replaced after its metadata was inspected.
    with path.open("rb") as stream:
        raw = stream.read(limit + 1)
    require(len(raw) <= limit, "JSON evidence exceeds read bound")
    return raw.decode("utf-8-sig")


def read_json(path, limit=JSON_LIMIT):
    return strict_json(bounded_read(path, limit))


def read_probe(path):
    return [strict_json(line) for line in bounded_read(path, PROBE_LIMIT).splitlines()]


# This module owns its separately loaded native validator instance. Route its
# nested build-identity.json read through the same strict bounded reader; the
# shared launcher source and other imports are not modified.
native.read_json = read_json


def fields(value, keys):
    require(isinstance(value, dict) and set(value) == set(keys.split()), "Unexpected or missing record fields")


def text(value):
    require(isinstance(value, str) and 0 < len(value.strip()) <= 2048 and
            value.strip().lower() not in {"planned", "pending", "unknown", "unverified", "todo"},
            "Actual observation text is required")


def instant(value):
    result = native.timestamp(value)
    require(result.tzinfo is not None and result.utcoffset().total_seconds() == 0,
            "Observation timestamp must be UTC")
    return result


def reference(value, retained, root=None, limit=None):
    fields(value, "path sha256")
    path = Path(value["path"])
    native.regular(path)
    if root is not None:
        require(path.resolve().is_relative_to(root.resolve()), "Evidence belongs to another session")
    if limit is not None:
        require(path.stat().st_size <= limit, "Evidence exceeds size bound")
    require(isinstance(value["sha256"], str) and native.HASH.fullmatch(value["sha256"]) and
            native.digest(path) == value["sha256"], "Evidence hash mismatch")
    require(str(path) not in retained or retained[str(path)] == value["sha256"], "Evidence was replaced between steps")
    retained[str(path)] = value["sha256"]
    return path


def snapshot(value, session, retained, earliest, now):
    fields(value, "atUtc semanticState hwnd tuple probe capture privacy")
    text(value["semanticState"])
    moment = instant(value["atUtc"])
    require(earliest <= moment <= now, "Observation outside build/time bounds")
    hwnd = value["hwnd"]
    require(type(hwnd) is int and hwnd > 0, "Native window identity is missing")
    fields(value["tuple"], "language dark density dpiScale client motion")
    fields(value["tuple"]["client"], "w h")
    require(all(type(value["tuple"]["client"][k]) is int for k in ("w", "h")), "Invalid client dimensions")
    require(value["tuple"]["motion"] in ("normal", "reduced"), "Observed motion policy is missing")
    probe_path = reference(value["probe"], retained, session["root"], PROBE_LIMIT)
    rows = read_probe(probe_path)
    # Reuse the native version-1 initial profile contract. Other matrix tuples
    # need a separately reviewed native validator, not a looser ledger claim.
    geometry = native.validate_probe(rows, session["pid"], hwnd, session["id"])
    require(value["tuple"]["language"] == "en" and value["tuple"]["dark"] is False and
            value["tuple"]["density"] == "comfortable" and
            type(value["tuple"]["dpiScale"]) in (int, float) and
            value["tuple"]["dpiScale"] == geometry["dpi_scale"] and
            value["tuple"]["client"] == {k: geometry["client"][k] for k in ("w", "h")},
            "Observed tuple differs from native probe")
    fields(value["capture"], "file reply")
    capture = reference(value["capture"]["file"], retained, session["root"], 64 * 1024 * 1024)
    fields(value["capture"]["reply"], "rendered_ok mode window_hwnd path")
    native.validate_image(capture, value["capture"]["reply"], hwnd, geometry["client"])
    fields(value["privacy"], "status reviewer reviewedAtUtc")
    require(value["privacy"]["status"] == "reviewed-safe", "Pixels require an explicit privacy review")
    text(value["privacy"]["reviewer"])
    require(moment <= instant(value["privacy"]["reviewedAtUtc"]) <= now,
            "Privacy review predates observation or is in the future")
    return moment, geometry


def validate(document, now=None):
    """Validate provenance consistency, not authenticity or visual acceptance."""
    now = now or datetime.now(timezone.utc)
    fields(document, "schemaVersion kind producer sourceCommit buildReceipt session steps")
    require(type(document["schemaVersion"]) is int and document["schemaVersion"] == 1 and
            document["kind"] == "local-native-interactions", "Unsupported interaction ledger")
    retained = {}
    build_path = reference(document["buildReceipt"], retained, limit=1024 * 1024)
    build = read_json(build_path)
    producer = Path(document["producer"])
    require(producer.is_absolute(), "Producer path must be absolute")
    native.validate_receipt(build, producer, document["sourceCommit"], now)
    fields(document["session"], "id review")
    review_path = reference(document["session"]["review"], retained, limit=16 * 1024 * 1024)
    root = review_path.parent
    require(root.parent.resolve() == Path(tempfile.gettempdir()).resolve() and
            root.name.startswith("bambu-local-review-") and review_path.name == "review.json" and
            document["session"]["id"] == root.name, "Not an owned local review session")
    review = read_json(review_path, PROBE_LIMIT)
    require(type(review.get("schemaVersion")) is int and review["schemaVersion"] == 1 and
            review.get("kind") == "local-initial-shell" and review.get("desktop") == "visible" and
            review.get("sourceCommit") == document["sourceCommit"] and
            review.get("buildReceiptSha256") == document["buildReceipt"]["sha256"] and
            review.get("driverSha256") == native.digest(HERE / "local-native-review.py") and
            review.get("teardown") == "verified" and "failure" not in review,
            "Session receipt is incomplete or belongs to another source/driver")
    launch = review["launch"]
    require(launch.get("status") == "started" and type(launch.get("pid")) is int and launch["pid"] > 0 and
            launch["shell"]["pid"] == launch["pid"] and launch["shell"]["visible"] is True and
            launch["shell"]["class"] == "wxWindowNR", "Owned native session was not observed")
    session = {"root": root, "id": root.name, "pid": launch["pid"]}
    require(review["probe"].get("status") == "received", "Initial native probe is missing")
    initial_probe = reference({"path": str(root / "shell.jsonl"), "sha256": review["probe"]["sha256"]}, retained, root, PROBE_LIMIT)
    native.validate_probe(read_probe(initial_probe),
                          session["pid"], launch["shell"]["hwnd"], session["id"])
    steps = document["steps"]
    require(isinstance(steps, list) and len(steps) <= 1000, "Invalid step inventory")
    ids, previous, findings = set(), None, []
    earliest = instant(build["finishedAtUtc"])
    for index, step in enumerate(steps, 1):
        fields(step, "id sequence status sourceCommit buildReceiptSha256 sessionId pid action pre post")
        text(step["id"])
        require(step["id"] not in ids and type(step["sequence"]) is int and step["sequence"] == index,
                "Repeated or out-of-order interaction")
        ids.add(step["id"])
        require(step["status"] == "observed" and step["sourceCommit"] == document["sourceCommit"] and
                step["buildReceiptSha256"] == document["buildReceipt"]["sha256"] and
                step["sessionId"] == session["id"] and type(step["pid"]) is int and step["pid"] == session["pid"],
                "Step is planned or has mismatched ownership")
        fields(step["action"], "method target atUtc")
        require(step["action"]["method"] in ("native-keyboard", "native-pointer"), "Native input method is required")
        text(step["action"]["target"])
        pre_time, pre_geometry = snapshot(step["pre"], session, retained, earliest, now)
        post_time, post_geometry = snapshot(step["post"], session, retained, earliest, now)
        require(pre_time < instant(step["action"]["atUtc"]) < post_time, "Interaction timestamps are out of order")
        require(previous is None or step["pre"] == previous, "Unobserved gap between consecutive interactions")
        require(Path(step["pre"]["probe"]["path"]).resolve() != Path(step["post"]["probe"]["path"]).resolve() and
                Path(step["pre"]["capture"]["file"]["path"]).resolve() != Path(step["post"]["capture"]["file"]["path"]).resolve(),
                "Pre/post evidence must be separately retained")
        previous = step["post"]
        findings.append({"step": step["id"], "pre": pre_geometry, "post": post_geometry})
    # Check again after every observation has been read. Never bless a moving
    # producer or silently retain a changed capture behind its earlier digest.
    native.validate_receipt(build, producer, document["sourceCommit"], now)
    for path, digest in retained.items():
        require(native.digest(Path(path)) == digest, "Evidence changed during validation")
    return {"schemaVersion": 1, "kind": "local-native-ledger-validation",
            "status": "evidence-consistent" if steps else "incomplete", "observedSteps": len(steps),
            "runtimeAcceptance": "unverified", "visualAcceptance": "unverified",
            "publication": "not_authorized", "document": document, "buildReceipt": build,
            "sessionReceipt": review, "retainedOriginals": retained, "nativeFindings": findings}


def save(report, evidence_root):
    """Create only a new, explicitly selected direct child of the OS temp root."""
    require(evidence_root.is_absolute() and evidence_root.parent.resolve() == Path(tempfile.gettempdir()).resolve()
            and evidence_root.name.startswith("bambu-ledger-"), "Output must be a new owned temporary directory")
    require(not any((parent / ".git").exists() for parent in (evidence_root.parent, *evidence_root.parent.parents)),
            "Output must remain outside repositories")
    evidence_root.mkdir(exist_ok=False)
    (evidence_root / "ledger.json").write_text(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--evidence-root", required=True, type=Path,
                        help="New absolute bambu-ledger-* directory directly under the OS temp root")
    args = parser.parse_args()
    report = validate(read_json(args.input, PROBE_LIMIT))
    save(report, args.evidence_root)
    # No paths, profile data, semantic content or raw exception text on stdout.
    print(json.dumps({key: report[key] for key in ("status", "observedSteps", "runtimeAcceptance", "publication")}))
    return 0 if report["observedSteps"] else 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, KeyError, TypeError, AttributeError, OSError, OverflowError, ImportError,
            native.subprocess.SubprocessError):
        print('{"status":"incomplete","reason":"Ledger validation or private output failed","runtimeAcceptance":"unverified"}', file=sys.stderr)
        raise SystemExit(2)
