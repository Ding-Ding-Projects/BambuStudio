#!/usr/bin/env python3
"""Validate an offline review plan. This module never drives an application."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

MAX_BYTES = 64 * 1024
CONTRACTS = (
    "design/workflow-refresh/built-review-queue.md",
    "design/workflow-refresh/implementation-scopes.json",
    "design/workflow-refresh/manifest.json",
)
SHA = re.compile(r"[0-9a-f]{40}")
# These are prospective operations, not executable commands or automation IDs.
MENU_ACTIONS = frozenset(("menu-next", "menu-previous", "menu-dismiss"))
MENU_STATES = frozenset(("menus/submenu", "menus/disabled", "menus/keyboard"))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def exact(value, fields, label):
    require(type(value) is dict and set(value) == set(fields), label + " fields differ")


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate JSON field")
        result[key] = value
    return result


def decode(raw, limit=MAX_BYTES):
    require(len(raw) <= limit, "JSON exceeds byte bound")
    try:
        return json.loads(raw.decode("utf-8"), object_pairs_hook=unique,
                          parse_constant=lambda _: (_ for _ in ()).throw(ValueError("Non-finite JSON value")))
    except (UnicodeError, RecursionError, json.JSONDecodeError) as exc:
        raise ValueError("Invalid JSON document") from exc


def contract_at(root, source):
    """Read only committed, fixed paths. Never trust the working copy as a pin."""
    require(type(source) is str and SHA.fullmatch(source), "Expected full source commit")
    result = {}
    for path in CONTRACTS:
        proc = subprocess.run(["git", "-C", str(root), "show", source + ":" + path],
                              capture_output=True, timeout=20, check=True)
        require(len(proc.stdout) <= 1024 * 1024, "Review contract exceeds byte bound")
        result[path] = proc.stdout
    return result


def queue_states(contracts):
    """Reuse queue boundary IDs and manifest state IDs, without treating them as controls."""
    scopes = decode(contracts[CONTRACTS[1]], 1024 * 1024)
    manifest = decode(contracts[CONTRACTS[2]], 1024 * 1024)
    ids = [row["id"] for row in scopes["remainingVisualCoverage"]]
    require(len(ids) == 9 and len(set(ids)) == 9, "Expected nine distinct review boundaries")
    known = {row["id"] + "/" + state for row in manifest["surfaces"] for state in row["states"]}
    queue = contracts[CONTRACTS[0]].decode("utf-8")
    sections = re.split(r"(?m)^### [1-9]\. `([a-z0-9-]+)`\s*$", queue)
    require(len(sections) == 19 and sections[1::2] == ids, "Queue boundary order differs")
    result = {}
    for boundary, text in zip(sections[1::2], sections[2::2]):
        candidates = set()
        for token in re.findall(r"`([^`\n]+)`", text):
            grouped = re.fullmatch(r"([a-z0-9-]+)/\{([a-z0-9,-]+)\}", token)
            if grouped:
                candidates.update(grouped[1] + "/" + state for state in grouped[2].split(","))
            elif re.fullmatch(r"[a-z0-9-]+/[a-z0-9-]+", token):
                candidates.add(token)
        result[boundary] = candidates & known
        require(result[boundary], "Queue boundary has no known state")
    return result


def validate_plan(plan, expected_source, contracts):
    exact(plan, ("schemaVersion", "kind", "sourceCommit", "contractSha256", "steps"), "Plan")
    require(type(plan["schemaVersion"]) is int and plan["schemaVersion"] == 1, "Unsupported schema")
    require(plan["kind"] == "native-review-preparation", "Only preparation plans are accepted")
    require(type(expected_source) is str and SHA.fullmatch(expected_source) and
            plan["sourceCommit"] == expected_source, "Reviewed source differs")
    hashes = {path: hashlib.sha256(contracts[path]).hexdigest() for path in CONTRACTS}
    exact(plan["contractSha256"], CONTRACTS, "Contract hash")
    require(plan["contractSha256"] == hashes, "Review contracts differ")
    states = queue_states(contracts)
    steps = plan["steps"]
    require(type(steps) is list and 1 <= len(steps) <= 64, "Expected 1 to 64 steps")
    duration = 0
    for step in steps:
        exact(step, ("boundary", "state", "action", "timeoutMs"), "Step")
        boundary, state, action = (step[key] for key in ("boundary", "state", "action"))
        require(all(type(value) is str for value in (boundary, state, action)), "Invalid step identifiers")
        require(boundary in states and state in states[boundary], "State is outside its reviewed boundary")
        require(action == "observe-native" or (action in MENU_ACTIONS and
                boundary == "shared-control-callers" and state in MENU_STATES), "Unsupported action")
        require(type(step["timeoutMs"]) is int and 100 <= step["timeoutMs"] <= 2000,
                "Step timeout must be 100 to 2000 ms")
        duration += step["timeoutMs"]
    require(duration <= 30000, "Plan exceeds 30000 ms budget")
    return {"kind": "native-review-preparation", "sourceCommit": expected_source,
            "validation": "passed", "execution": "not_attempted", "runtimeAcceptance": "unverified",
            "steps": len(steps), "timeoutMs": duration, "contractSha256": hashes}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", required=True, type=Path)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--plan", required=True, type=Path)
    args = parser.parse_args()
    # Bound the read itself, not only the later decoder.
    with args.plan.open("rb") as stream:
        plan = decode(stream.read(MAX_BYTES + 1))
    result = validate_plan(plan, args.source_commit, contract_at(args.repository, args.source_commit))
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, KeyError, TypeError, OSError, subprocess.SubprocessError):
        # Never reflect arbitrary plan text or local paths into diagnostics.
        print(json.dumps({"validation": "failed", "execution": "not_attempted",
                          "runtimeAcceptance": "unverified"}), file=sys.stderr)
        raise SystemExit(2)
