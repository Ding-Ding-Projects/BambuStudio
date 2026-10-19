#!/usr/bin/env python3
"""Wrap the optional preparation sections of the Bambu Lab start G-code templates in
``{if !express_print_mode}`` guards.

Usage:
    python3 scripts/profiles/express_start_gcode_guards.py            # apply (idempotent)
    python3 scripts/profiles/express_start_gcode_guards.py --check    # exit 1 if a file would change
    python3 scripts/profiles/express_start_gcode_guards.py --render express|normal "<template path>"

The express print mode process option keeps heating, one homing pass, the AMS
filament load, the purge line and the temperature waits. Everything the
printer firmware already gates behind a send-time flag (bed leveling,
extrusion calibration, toolhead offset calibration: ``M1002 judge_flag`` plus
``M622 J1`` / ``M622 J2`` blocks) is left untouched, because print setup sends
those flags off and the ``M622 J0`` branches still home. Only the sections
that run unconditionally are guarded here: the start sound, cog noise
reduction, the nozzle wipe, the vibration (mech mode) check, the scanner,
laser, camera and build plate detections and the z offset calibration.

Each rule names a start marker and an end marker (regular expressions matched
against the stripped line). The guard is inserted before the start line and
after the end line. A range must keep its ``{if}`` / ``{endif}`` placeholder
blocks and its ``M622`` / ``M623`` firmware blocks balanced, or the script
refuses to touch the file. The JSON string literal is re-encoded with
``json.dumps`` and spliced back into the original file text, which keeps the
surrounding formatting byte-for-byte.

Only standard-library Python is used.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MACHINE_DIR = ROOT / "resources" / "profiles" / "BBL" / "machine"
KEY = "machine_start_gcode"
GUARD_OPEN = "{if !express_print_mode}"
GUARD_CLOSE = "{endif}"
HEADER = [
    "{if express_print_mode}",
    "; express print mode: preparation skipped except homing, filament loading, the purge line and the temperature waits",
    "{endif}",
]

SOUND = r"^;=+\s*(start printer sound|printer start sound)"
MARKER = r"^;="


def rule(name, start, end=None, *, end_before=None, required=True, repeat_closer=False):
    """A guard rule.

    ``end`` is the inclusive end marker. ``end_before`` ends the range on the
    line before the first later match. A rule with neither guards one line.
    ``repeat_closer`` ends on the next line that matches ``start`` again.
    """
    return {
        "name": name,
        "start": re.compile(start),
        "end": re.compile(end) if end else None,
        "end_before": re.compile(end_before) if end_before else None,
        "required": required,
        "repeat_closer": repeat_closer,
    }


def sound_rule(required=True):
    return rule("start sound", SOUND, end_before=MARKER, required=required)


COG = rule("cog noise reduction", r"^M982\.2 S1")

X1_RULES = [
    sound_rule(required=False),  # X1E has no sound block
    rule("wipe nozzle", r"^;=+\s*wipe nozzle\s*=", r"^;=+\s*wipe nozzle end"),
    rule("check scanner clarity", r"^;=+\s*check scanner clarity\s*=", r"^;=+\s*check scanner clarity end"),
    rule("mech mode fast check", r"^;=+\s*mech mode fast check", repeat_closer=True),
    rule("laser and rgb calibration", r"^;=+\s*laser and rgb calibration", end_before=r"^;=+\s*handeye calibration"),
]
P1_RULES = [
    rule("wipe nozzle", r"^;=+\s*wipe nozzle\s*=", r"^;=+\s*wipe nozzle end"),
    rule("mech mode fast check", r"^;=+\s*mech mode fast check", r"^;=+\s*f?mech mode fast check"),
]
A1_RULES = [
    sound_rule(),
    COG,
    rule("build plate detection", r"^M1002 judge_flag build_plate_detect_flag", r"^M623"),
    rule("mech mode fast check", r"^;=+\s*mech mode fast check start", r"^;=+\s*mech mode fast check end"),
    rule("wipe nozzle", r"^;=+\s*wipe nozzle\s*=", r"^;=+\s*wipe nozzle end"),
]
A1_MINI_RULES = [
    sound_rule(),
    COG,
    rule("mech mode fast check", r"^;=+\s*mech mode fast check", end_before=r"^;=+\s*wipe nozzle\s*="),
    rule("wipe nozzle", r"^;=+\s*wipe nozzle\s*=", r"^;=+\s*wipe nozzle end"),
]
A2L_RULES = [
    sound_rule(),
    COG,
    rule("detection", r"^;=+\s*detection start", r"^;=+\s*detection end"),
    rule("wipe nozzle", r"^;=+\s*wipe nozzle start", r"^;=+\s*wipe nozzle end"),
    rule("mech mode sweep", r"^;=+\s*mech mode sweep start", r"^;=+\s*mech mode sweep end"),
]
H2_COMMON = [
    rule("detection", r"^;=+\s*detection start", r"^;=+\s*detection end"),
    rule("wipe nozzles", r"^;=+\s*wipe right nozzle start", r"^;=+\s*wipe left nozzle end"),
    rule("z offset calibration", r"^;=+\s*z ofst cali start", r"^;=+\s*z ofst cali end", required=False),
    rule("mech mode sweep", r"^;=+\s*mech mode sweep start", r"^;=+\s*mech mode sweep end"),
]
H2D_RULES = [
    sound_rule(),
    COG,
    rule("first homing checks", r"^M972 S24 P0 T2000", r"^M972 S41 P0 T5000"),
] + H2_COMMON
H2C_RULES = [
    sound_rule(),
    COG,
    rule("first homing camera checks", r"^M972 S24 P0 T2000", r"^M972 S46 P0 T5000"),
    rule("first homing heatbed checks", r"^M1002 gcode_claim_action : 74", r"^M972 S41 P0 T5000"),
] + H2_COMMON
X2D_RULES = [
    sound_rule(),
    COG,
    rule("first homing checks", r"^M972 S24 P0$", r"^M972 S26 P0 C0"),
] + H2_COMMON
P2S_RULES = [
    sound_rule(),
    COG,
    rule("first homing checks", r"^M972 S24 P0$", r"^M972 S42 P0 T5000"),
] + H2_COMMON

# Order matters: the first matching family wins.
FAMILIES = [
    ("Bambu Lab X1 Carbon ", X1_RULES),
    ("Bambu Lab X1E ", X1_RULES),
    ("Bambu Lab X1 ", X1_RULES),
    ("Bambu Lab P1P ", P1_RULES),
    ("Bambu Lab P1S ", P1_RULES),
    ("Bambu Lab A1 mini ", A1_MINI_RULES),
    ("Bambu Lab A1 ", A1_RULES),
    ("Bambu Lab A2L ", A2L_RULES),
    ("Bambu Lab H2D Pro ", H2D_RULES),
    ("Bambu Lab H2D ", H2D_RULES),
    ("Bambu Lab H2C ", H2C_RULES),
    ("Bambu Lab H2S ", H2D_RULES),
    ("Bambu Lab P2S ", P2S_RULES),
    ("Bambu Lab X2D ", X2D_RULES),
]


class GuardError(Exception):
    pass


def family_rules(path: Path):
    for prefix, rules in FAMILIES:
        if path.name.startswith(prefix):
            return prefix.strip(), rules
    raise GuardError(f"{path.name}: no family rules")


def template_files():
    return sorted(MACHINE_DIR.glob("Bambu Lab * template machine_start_gcode.json"))


def check_balanced(lines, name):
    depth = 0
    for line in lines:
        s = line.strip()
        if s.startswith("{if"):
            depth += 1
        elif s.startswith("{endif"):
            depth -= 1
        elif s.startswith("{else") or s.startswith("{elsif"):
            if depth == 0:
                raise GuardError(f"{name}: {{else}} at depth 0 inside a guarded range")
        if depth < 0:
            raise GuardError(f"{name}: unbalanced {{endif}} inside a guarded range")
    if depth != 0:
        raise GuardError(f"{name}: unbalanced {{if}} inside a guarded range")
    opened = sum(1 for l in lines if l.strip().startswith("M622"))
    closed = sum(1 for l in lines if l.strip().startswith("M623"))
    if opened != closed:
        raise GuardError(f"{name}: M622/M623 pair split by a guarded range")
    if any(l.strip() == GUARD_OPEN for l in lines):
        raise GuardError(f"{name}: nested express guard")


def guarded_indices(lines):
    """Indices of every line that already sits inside an express guard."""
    inside = set()
    depth = 0
    for i, line in enumerate(lines):
        s = line.strip()
        if depth:
            inside.add(i)
            if s.startswith("{if"):
                depth += 1
            elif s.startswith("{endif"):
                depth -= 1
        elif s == GUARD_OPEN:
            depth = 1
            inside.add(i)
    return inside


def find_range(lines, r, name):
    start = None
    inside = guarded_indices(lines)
    for i, line in enumerate(lines):
        if i in inside:
            continue
        if r["start"].search(line.strip()):
            start = i
            break
    if start is None:
        already = any(i in inside and r["start"].search(l.strip()) for i, l in enumerate(lines))
        if r["required"] and not already:
            raise GuardError(f"{name}: required rule '{r['name']}' did not match")
        return None
    if r["repeat_closer"]:
        pat, inclusive = r["start"], True
    elif r["end"] is not None:
        pat, inclusive = r["end"], True
    elif r["end_before"] is not None:
        pat, inclusive = r["end_before"], False
    else:
        return start, start
    for j in range(start + 1, len(lines)):
        if pat.search(lines[j].strip()):
            if inclusive:
                return start, j
            # The sound block closes with a repeated sound marker on some families.
            if r["name"] == "start sound" and re.search(SOUND, lines[j].strip()):
                return start, j
            return start, j - 1
    raise GuardError(f"{name}: rule '{r['name']}' has no end marker")


def apply_rules(gcode: str, rules, name: str) -> str:
    lines = gcode.split("\n")
    if lines[: len(HEADER)] != HEADER:
        lines = HEADER + lines
    for r in rules:
        rng = find_range(lines, r, name)
        if rng is None:
            continue
        a, b = rng
        check_balanced(lines[a : b + 1], f"{name} / {r['name']}")
        lines = lines[:a] + [GUARD_OPEN] + lines[a : b + 1] + [GUARD_CLOSE] + lines[b + 1 :]
    return "\n".join(lines)


def read_template(path: Path):
    text = path.read_text(encoding="utf-8")
    data = json.loads(text)
    literal = json.dumps(data[KEY], ensure_ascii=False)
    if text.count(literal) != 1:
        raise GuardError(f"{path.name}: the {KEY} literal is not reproduced byte-for-byte")
    return text, data[KEY], literal


def guarded_text(path: Path) -> tuple[str, str]:
    text, gcode, literal = read_template(path)
    _, rules = family_rules(path)
    new_gcode = apply_rules(gcode, rules, path.name)
    return text, text.replace(literal, json.dumps(new_gcode, ensure_ascii=False))


def render(gcode: str, mode: str) -> str:
    """Resolve only the express guards and the firmware flag blocks.

    ``express`` drops ``{if !express_print_mode}`` bodies and ``M622 J1`` /
    ``M622 J2`` bodies (print setup sends those flags off); ``normal`` drops
    the ``{if express_print_mode}`` bodies. Other placeholders stay as they are.
    """
    out = []
    skip_guard = 0  # depth counter of dropped placeholder blocks
    keep_stack = []  # placeholder blocks that were resolved and kept
    skip_fw = 0
    for line in gcode.split("\n"):
        s = line.strip()
        if skip_guard:
            if s.startswith("{if"):
                skip_guard += 1
            elif s.startswith("{endif"):
                skip_guard -= 1
            continue
        if s == GUARD_OPEN:
            if mode == "express":
                skip_guard = 1
            else:
                keep_stack.append(True)
            continue
        if s == "{if express_print_mode}":
            if mode == "express":
                keep_stack.append(True)
            else:
                skip_guard = 1
            continue
        if s.startswith("{endif") and keep_stack and _closes_kept(keep_stack):
            keep_stack.pop()
            continue
        if mode == "express":
            if skip_fw:
                if s.startswith("M622"):
                    skip_fw += 1
                elif s.startswith("M623"):
                    skip_fw -= 1
                continue
            if re.match(r"^M622 J[12]\b", s):
                skip_fw = 1
                continue
        out.append(line)
        if s.startswith("{if"):
            keep_stack.append(False)
        elif s.startswith("{endif") and keep_stack:
            keep_stack.pop()
    return "\n".join(out)


def _closes_kept(stack):
    return stack[-1] is True


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true", help="report files that would change and exit 1")
    ap.add_argument("--render", choices=["express", "normal"], help="print one template resolved for the mode")
    ap.add_argument("path", nargs="?", help="template to render (with --render)")
    args = ap.parse_args(argv)

    if args.render:
        if not args.path:
            ap.error("--render needs a template path")
        _, gcode, _ = read_template(Path(args.path))
        print(render(gcode, args.render))
        return 0

    changed = []
    try:
        for path in template_files():
            text, new_text = guarded_text(path)
            if new_text != text:
                changed.append(path)
                if not args.check:
                    path.write_text(new_text, encoding="utf-8")
    except GuardError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    if args.check:
        for path in changed:
            print(f"would change: {path.relative_to(ROOT)}")
        return 1 if changed else 0
    for path in changed:
        print(f"guarded: {path.relative_to(ROOT)}")
    print(f"{len(changed)} file(s) changed, {len(template_files())} template(s) checked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
