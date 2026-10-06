"""Exercise exported source with real SQLite, Python and Node readers."""
import json
from pathlib import Path
import runpy
import sqlite3
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="export-roundtrip-") as scratch:
    root = Path(scratch)
    subprocess.run([sys.argv[1], scratch], check=True)
    baseline_text = (root / "fixture.json").read_text(encoding="utf-8")
    baseline = json.loads(baseline_text)
    db = sqlite3.connect(":memory:")
    db.executescript((root / "fixture.sql").read_text(encoding="utf-8"))
    assert db.execute("SELECT document_utf8 FROM export_snapshot").fetchone()[0].decode("utf-8") == baseline_text
    assert runpy.run_path(str(root / "fixture.py"))["snapshot"] == baseline
    module = root / "fixture.mjs"
    module.write_bytes((root / "fixture.js").read_bytes())
    node = subprocess.run(["node", "--input-type=module", "-e", "import {json} from " + json.dumps(module.as_uri()) + ";process.stdout.write(json)"], check=True, capture_output=True)
    assert node.stdout.decode("utf-8") == baseline_text
    assert json.loads((root / "fixture.schema.json").read_text(encoding="utf-8"))["const"] == baseline
    # These grammars share the generated string-literal subset. Runtime toolchains
    # for TypeScript, Go, Rust and Protobuf remain a separate validation obligation.
    for ext, prefix in [("ts", "export const json: string = "), ("go", "const JSON = "), ("rs", "pub const JSON: &str = "), ("textproto", "json_utf8: ")]:
        line = next(line for line in (root / ("fixture." + ext)).read_text(encoding="utf-8").splitlines() if line.startswith(prefix))
        assert json.loads(line[len(prefix):].removesuffix(";")) == baseline_text
    print("Portable round trips passed: SQLite, Python, Node, exact JSON Schema; four additional literal payload checks. Full Go/Rust/TypeScript/Protobuf toolchain validation not performed.")
