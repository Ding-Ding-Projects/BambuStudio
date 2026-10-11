#!/usr/bin/env python3
"""Capture what a real LAN model drop container answers, for container_fixtures.test.mjs.

Builds lan-model-drop/ and starts it with docker compose under a project name and port of its own,
then:

* opens the sender page in headless Chromium from an invite link and sends every sample model
  through it (container_page.test.mjs records each request the page really makes);
* sends every sample model again with curl, with exactly the protocol headers the page sent, and
  sends the sender-side mistakes the page guards against (wrong code, wrong type, no length ...);
* drives the station API the way Bambu Studio does (the station key as a Bearer header, Accept:
  application/json, no proxy): status, inbox, every file, wrong and missing keys, a new drop code,
  DELETE twice, and the empty inbox;
* starts the container a second time with every setting changed (fixed code, public URL, a 60
  character station name, a fraction of an hour, tiny limits), and runs the service with settings
  it must refuse;

and writes every answer (raw header lines as libcurl hands them to the station, and the body) with
a manifest into tests/lan_model_drop/container_fixtures/. The container and its volume are removed
at the end. The station key is read with `docker compose exec` and never written anywhere.

    python3 tests/lan_model_drop/container_capture.py [--port 18851] [--project lan-drop-capture]
        [--out tests/lan_model_drop/container_fixtures] [--chromium PATH]

Needs docker with compose, curl, node and (for the page) Chromium. Only localhost is contacted.
"""

import argparse
import datetime
import hashlib
import io
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import urllib.parse
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DROP = REPO / "lan-model-drop"
FIXED_TIME = (2026, 10, 9, 8, 0, 0)

# ----------------------------------------------------------------------------------- sample models

TETRA = [((0, 0, 0), (0, 10, 0), (10, 0, 0)), ((0, 0, 0), (10, 0, 0), (0, 0, 10)),
         ((0, 0, 0), (0, 0, 10), (0, 10, 0)), ((10, 0, 0), (0, 10, 0), (0, 0, 10))]

MODEL_3MF = """<?xml version="1.0" encoding="UTF-8"?>
<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">
 <resources>
  <object id="1" type="model">
   <mesh>
    <vertices>
     <vertex x="0" y="0" z="0"/><vertex x="10" y="0" z="0"/><vertex x="0" y="10" z="0"/><vertex x="0" y="0" z="10"/>
    </vertices>
    <triangles>
     <triangle v1="0" v2="2" v3="1"/><triangle v1="0" v2="1" v3="3"/><triangle v1="0" v2="3" v3="2"/><triangle v1="1" v2="2" v3="3"/>
    </triangles>
   </mesh>
  </object>
 </resources>
 <build><item objectid="1"/></build>
</model>
"""

CONTENT_TYPES = """<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
 <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
 <Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>
</Types>
"""

RELS = """<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
 <Relationship Target="/3D/3dmodel.model" Id="rel0" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>
</Relationships>
"""

STEP = ("ISO-10303-21;\r\nHEADER;\r\nFILE_DESCRIPTION(('tetrahedron'),'2;1');\r\n"
        "FILE_NAME('{name}','2026-10-09T08:00:00',(''),(''),'','','');\r\n"
        "FILE_SCHEMA(('AUTOMOTIVE_DESIGN'));\r\nENDSEC;\r\nDATA;\r\nENDSEC;\r\nEND-ISO-10303-21;\r\n")

OBJ = "# tetrahedron\no tetra\nv 0 0 0\nv 10 0 0\nv 0 10 0\nv 0 0 10\nf 1 3 2\nf 1 2 4\nf 1 4 3\nf 2 3 4\n"

AMF = """<?xml version="1.0" encoding="UTF-8"?>
<amf unit="millimeter" version="1.1">
 <object id="1">
  <mesh>
   <vertices>
    <vertex><coordinates><x>0</x><y>0</y><z>0</z></coordinates></vertex>
    <vertex><coordinates><x>10</x><y>0</y><z>0</z></coordinates></vertex>
    <vertex><coordinates><x>0</x><y>10</y><z>0</z></coordinates></vertex>
    <vertex><coordinates><x>0</x><y>0</y><z>10</z></coordinates></vertex>
   </vertices>
   <volume>
    <triangle><v1>0</v1><v2>2</v2><v3>1</v3></triangle>
    <triangle><v1>0</v1><v2>1</v2><v3>3</v3></triangle>
    <triangle><v1>0</v1><v2>3</v2><v3>2</v3></triangle>
    <triangle><v1>1</v1><v2>2</v2><v3>3</v3></triangle>
   </volume>
  </mesh>
 </object>
</amf>
"""


def zip_bytes(entries):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, data in entries:
            info = zipfile.ZipInfo(name, FIXED_TIME)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            archive.writestr(info, data)
    return buffer.getvalue()


def ascii_stl(name, copies=1):
    lines = [f"solid {name}"]
    for _ in range(copies):
        for a, b, c in TETRA:
            lines += [" facet normal 0 0 0", "  outer loop"]
            lines += [f"   vertex {x} {y} {z}" for x, y, z in (a, b, c)]
            lines += ["  endloop", " endfacet"]
    lines.append(f"endsolid {name}")
    return ("\n".join(lines) + "\n").encode("ascii")


def binary_stl():
    header = b"solid binary tetrahedron written by the capture tool".ljust(80, b" ")
    body = struct.pack("<I", len(TETRA))
    for tri in TETRA:
        body += struct.pack("<3f", 0, 0, 0)
        for vertex in tri:
            body += struct.pack("<3f", *vertex)
        body += struct.pack("<H", 0)
    return header + body


# One model of every accepted type and form: (file in the fixtures, name as a person chose it, type,
# bytes). The names are what the page sends; the files are named in ASCII for every checkout.
SAMPLES = [
    ("bracket-v2.3mf", "Bracket v2.3mf", "3mf",
     zip_bytes([("[Content_Types].xml", CONTENT_TYPES), ("_rels/.rels", RELS), ("3D/3dmodel.model", MODEL_3MF)])),
    ("cube-ascii.stl", "\u7acb\u65b9\u9ad4 cube.stl", "stl", ascii_stl("tetrahedron")),
    ("part-binary.stl", "binary part.STL", "stl", binary_stl()),
    ("housing.step", "housing.step", "step", STEP.format(name="housing.step").encode("ascii")),
    ("clip-bom.stp", "clip.stp", "step", b"\xef\xbb\xbf" + STEP.format(name="clip.stp").encode("ascii")),
    ("mesh.obj", "mesh.obj", "obj", OBJ.encode("ascii")),
    ("model.amf", "model.amf", "amf", AMF.encode("ascii")),
    ("zipped-model.amf", "zipped model.amf", "amf", zip_bytes([("model.amf", AMF)])),
]

# Senders the curl drops give, one per sample, already as the page sends them (white space runs
# made one space and trimmed).
CURL_SENDERS = ["Ada", "\u9673\u5927\u6587", "Bob \U0001f642", "", "Mo Li", "x" * 40, "Lab PC 2", ""]
PAGE_SENDER = "  Mo   Li  "


def encode_uri_component(text):
    """JavaScript's encodeURIComponent."""
    return urllib.parse.quote(text, safe="-_.!~*'()")


# ------------------------------------------------------------------------------------- the runner


class Capture:
    def __init__(self, args):
        self.args = args
        self.out = Path(args.out).resolve()
        self.base = f"http://127.0.0.1:{args.port}"
        self.key = None
        self.manifest = {
            "about": "Answers of a real lan-model-drop container (docker compose), captured by "
                     "tests/lan_model_drop/container_capture.py and read by container_fixtures.test.mjs "
                     "and lan_model_drop_container_interop_test.cpp. Each exchange is the raw response "
                     "header lines as libcurl hands them to the station (.head) and the body (.body).",
            "protocol": 1,
            "samples": [],
            "scenarios": [],
            "refusedSettings": [],
        }
        self.override = None
        self.scenario = None
        self.counter = 0
        self.keys = []

    # --------------------------------------------------------------------------- docker compose

    def compose(self, *extra, env=None, check=True, capture=False, timeout=600):
        command = ["docker", "compose", "-p", self.args.project, "-f", str(DROP / "compose.yaml"), "-f", self.override, *extra]
        full_env = {**os.environ, "DROP_PORT": str(self.args.port), **(env or {})}
        result = subprocess.run(command, cwd=DROP, env=full_env, check=False, timeout=timeout,
                                stdout=subprocess.PIPE if capture else None, stderr=subprocess.PIPE if capture else None)
        if check and result.returncode != 0:
            raise SystemExit(f"docker compose {' '.join(extra)} failed ({result.returncode})")
        return result

    def up(self, env):
        self.compose("up", "-d", "--build", "--wait", "--wait-timeout", "120", env=env)
        key = self.compose("exec", "-T", "lan-model-drop", "node", "server/station-key.mjs", env=env, capture=True)
        self.key = key.stdout.decode("ascii").strip()
        assert 16 <= len(self.key) <= 512, "the station key is read"
        self.keys.append(self.key)

    def down(self):
        self.compose("down", "-v", "--remove-orphans", check=False)
        self.key = None

    # ---------------------------------------------------------------------------------- requests

    def file(self, label, suffix):
        return self.out / self.scenario["name"] / f"{label}{suffix}"

    def curl(self, label, path, *, method="GET", key=None, headers=(), data=None, verbose=False, record_request=False):
        """One request; returns the exchange record. `key`: 'station', 'wrong' or None."""
        self.counter += 1
        label = f"{self.counter:02d}-{label}"
        head, body = self.file(label, ".head"), self.file(label, ".body")
        command = ["curl", "-sS", "--noproxy", "*", "--max-time", "60", "-o", str(body), "-D", str(head), "-X", method]
        if key == "station":
            command += ["-H", f"Authorization: Bearer {self.key}"]
        elif key == "wrong":
            command += ["-H", "Authorization: Bearer not-the-station-key-0123456789"]
        for header in headers:
            command += ["-H", header]
        if data is not None:
            command += ["--data-binary", data]
        if verbose:
            command.append("-v")
        command.append(self.base + path)
        result = subprocess.run(command, check=False, capture_output=True, timeout=120)
        if result.returncode != 0:
            raise SystemExit(f"curl {label} failed: {result.stderr.decode(errors='replace')}")
        status_line = head.read_bytes().split(b"\r\n", 1)[0].decode("ascii")
        record = {
            "label": label,
            "request": f"{method} {path}",
            "status": int(status_line.split()[1]),
            "head": head.relative_to(self.out).as_posix(),
            "body": body.relative_to(self.out).as_posix(),
        }
        if record_request:
            # The header lines curl really sent (its -v trace), never with a key: only drops record them.
            assert key is None
            lines = [line[2:] for line in result.stderr.decode("utf-8").splitlines() if line.startswith("> ")]
            record["requestLines"] = [line for line in lines if line]
        return record

    def station(self, label, path, method="GET", key="station"):
        headers = ["Accept: application/json"]
        data = None
        if method == "POST":
            headers.append("Content-Type: application/json")
            data = "{}"
        return self.curl(label, path, method=method, key=key, headers=headers, data=data)

    def drop(self, label, *, code, name, data_file, sender="", content_type="", extra=(), expect=None):
        """POST /api/drop exactly as the sender page does: the raw bytes, X-Drop-Code, X-Drop-Filename
        and X-Drop-Sender (encodeURIComponent, the sender only when given), the browser's
        Content-Type for the file (none when it has none), Origin and Accept, and no Expect."""
        headers = ["Expect:", "Accept: */*", f"Origin: {self.base}", f"Content-Type: {content_type}" if content_type else "Content-Type:",
                   f"X-Drop-Code: {code}", f"X-Drop-Filename: {encode_uri_component(name)}"]
        if sender:
            headers.append(f"X-Drop-Sender: {encode_uri_component(sender)}")
        record = self.curl(label, "/api/drop", method="POST", headers=[*headers, *extra], data=f"@{data_file}", verbose=True,
                           record_request=True)
        record.update({"via": "curl", "name": name, "sender": sender, "contentType": content_type, "expect": expect or {}})
        return record

    def status_body(self, record):
        return json.loads((self.out / record["body"]).read_text("utf-8"))

    # ------------------------------------------------------------------------------- the samples

    def write_samples(self):
        folder = self.out / "samples"
        folder.mkdir(parents=True)
        for file_name, name, kind, data in SAMPLES:
            (folder / file_name).write_bytes(data)
            self.manifest["samples"].append({
                "file": f"samples/{file_name}", "name": name, "type": kind, "bytes": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
            })

    def sample_path(self, sample):
        return self.out / sample["file"]

    # ----------------------------------------------------------------------------- the page run

    def chromium(self):
        if self.args.chromium:
            return self.args.chromium
        browsers = os.environ.get("PLAYWRIGHT_BROWSERS_PATH", "")
        if browsers and Path(browsers).is_dir():
            for entry in sorted(Path(browsers).glob("chromium-*"), reverse=True):
                candidate = entry / "chrome-linux" / "chrome"
                if candidate.exists():
                    return str(candidate)
        return None

    def page_run(self, code):
        chromium = self.chromium()
        if not chromium:
            print("no Chromium: the page run is skipped", file=sys.stderr)
            return None
        with tempfile.TemporaryDirectory(prefix="lan-drop-capture-") as work:
            chosen = Path(work) / "chosen"
            chosen.mkdir()
            for sample in self.manifest["samples"]:
                shutil.copyfile(self.sample_path(sample), chosen / sample["name"])
            record_file = Path(work) / "page.json"
            env = {**os.environ, "LAN_DROP_PAGE_BASE": self.base, "LAN_DROP_PAGE_CODE": code, "LAN_DROP_PAGE_FILES": str(chosen),
                   "LAN_DROP_PAGE_SENDER": PAGE_SENDER, "LAN_DROP_PAGE_OUT": str(record_file), "CHROMIUM_PATH": chromium}
            run = subprocess.run(["node", "--test", str(REPO / "tests" / "lan_model_drop" / "container_page.test.mjs")], env=env,
                                 cwd=REPO, check=False, capture_output=True, timeout=600)
            if run.returncode != 0:
                raise SystemExit(f"the page run failed:\n{run.stdout.decode(errors='replace')}")
            page = json.loads(record_file.read_text("utf-8"))
        # Drop the headers that only describe this browser and machine.
        keep = {"content-length", "content-type", "x-drop-code", "x-drop-filename", "x-drop-sender", "origin", "accept",
                "expect", "transfer-encoding", "authorization", "host"}
        drops = []
        for request in page["requests"]:
            headers = {name: value for name, value in request["requestHeaders"].items() if name.lower() in keep}
            name = urllib.parse.unquote(headers.get("X-Drop-Filename", ""))
            drops.append({"via": "page", "name": name, "status": request["status"], "requestHeaders": headers,
                          "responseBody": request["body"]})
        page_record = {"invite": {"code": page["invite"]["code"] == code, "fragmentRemoved": page["invite"]["hash"] == ""},
                       "sender": PAGE_SENDER, "rows": page["rows"], "live": page["live"], "drops": drops}
        return page_record

    # ------------------------------------------------------------------------------- scenarios

    def begin(self, name, env):
        self.scenario = {"name": name, "env": env, "port": self.args.port}
        (self.out / name).mkdir(parents=True)
        self.counter = 0
        self.manifest["scenarios"].append(self.scenario)
        self.scenario["startedAt"] = utc_now()

    def default_scenario(self):
        self.begin("default", {})
        self.up({})
        s = self.scenario
        s["health"] = self.curl("health", "/healthz")
        s["statusEmpty"] = self.station("status-empty", "/api/station/status")
        code = self.status_body(s["statusEmpty"])["dropCode"]
        s["dropCode"] = code

        # 1. The page sends every sample.
        s["page"] = self.page_run(code)
        content_types = {}
        if s["page"]:
            for drop in s["page"]["drops"]:
                content_types[drop["name"]] = drop["requestHeaders"].get("Content-Type", "")

        # 2. curl sends every sample again, with the page's headers.
        s["drops"] = []
        for index, sample in enumerate(self.manifest["samples"]):
            sender = CURL_SENDERS[index]
            s["drops"].append(self.drop(f"drop-{sample['type']}", code=code, name=sample["name"], data_file=self.sample_path(sample),
                                        sender=sender, content_type=content_types.get(sample["name"], ""),
                                        expect={"status": 201, "sample": sample["file"], "stationFileName": sample["name"],
                                                "stationSender": sender}))
        # 3. Names the drop box cleans or refuses, with the station's reading of each.
        stl = self.manifest["samples"][1]
        names = [
            ('Part: "v3"?.stl', 201, "Part v3.stl"),
            ("CON.stl", 201, "_CON.stl"),
            ("COM3 .x.stl", 201, "_COM3 .x.stl"),
            ("a\u202eb.stl", 201, "ab.stl"),
            ("\u202e\u202e.stl", 400, None),
            ("\u200b.stl", 400, None),
        ]
        for name, status, station_name in names:
            expect = {"status": status}
            if status == 201:
                expect.update({"sample": stl["file"], "stationFileName": station_name, "stationSender": ""})
            else:
                expect.update({"error": "bad_request", "siteReason": "reasonName"})
            s["drops"].append(self.drop("drop-name", code=code, name=name, data_file=self.sample_path(stl), content_type="model/stl",
                                        expect=expect))
        # 4. Sender-side mistakes.
        with tempfile.TemporaryDirectory(prefix="lan-drop-capture-") as work:
            notes = Path(work) / "notes.txt"
            notes.write_bytes(b"plain notes, not a model\n")
            fake = Path(work) / "fake.3mf"
            fake.write_bytes(b"plain text, not a 3MF archive\n")
            wrong = "000000" if code != "000000" else "111111"
            s["drops"].append(self.drop("drop-wrong-code", code=wrong, name=stl["name"], data_file=self.sample_path(stl),
                                        content_type="model/stl",
                                        expect={"status": 401, "error": "wrong_code", "siteReason": "reasonWrongCode", "siteStop": True}))
            s["drops"].append(self.drop("drop-not-a-model", code=code, name="notes.txt", data_file=notes, content_type="text/plain",
                                        expect={"status": 415, "error": "unsupported_type", "siteReason": "reasonType"}))
            s["drops"].append(self.drop("drop-fake-3mf", code=code, name="fake.3mf", data_file=fake, content_type="model/3mf",
                                        expect={"status": 415, "error": "unsupported_type", "siteReason": "reasonType"}))
            s["drops"].append(self.drop("drop-long-sender", code=code, name=stl["name"], data_file=self.sample_path(stl),
                                        sender="y" * 41, content_type="model/stl",
                                        expect={"status": 400, "error": "bad_request", "siteReason": "reasonName"}))
            s["drops"].append(self.drop("drop-no-length", code=code, name=stl["name"], data_file=self.sample_path(stl),
                                        content_type="model/stl", extra=["Transfer-Encoding: chunked"],
                                        expect={"status": 411, "error": "length_required", "siteReason": "reasonServer"}))

        # 5. The station: status, inbox, every file.
        s["status"] = self.station("status", "/api/station/status")
        s["inbox"] = self.station("inbox", "/api/station/inbox")
        items = self.status_body(s["inbox"])["items"]
        s["files"] = [self.station("file", f"/api/station/files/{item['id']}") for item in items]
        s["statusWrongKey"] = self.station("status-wrong-key", "/api/station/status", key="wrong")
        s["inboxNoKey"] = self.station("inbox-no-key", "/api/station/inbox", key=None)
        s["fileNoKey"] = self.station("file-no-key", f"/api/station/files/{items[0]['id']}", key=None)
        s["fileUnknown"] = self.station("file-unknown", "/api/station/files/" + "0" * 32)
        # 6. A new drop code; the old one stops working.
        s["newCode"] = self.station("new-code", "/api/station/drop-code", method="POST")
        s["statusNewCode"] = self.station("status-new-code", "/api/station/status")
        stl_file = self.sample_path(stl)
        s["drops"].append(self.drop("drop-old-code", code=code, name=stl["name"], data_file=stl_file, content_type="model/stl",
                                    expect={"status": 401, "error": "wrong_code", "siteReason": "reasonWrongCode", "siteStop": True}))
        # 7. Open or Discard: DELETE, again, and the file is gone.
        s["deletes"] = [self.station("delete", f"/api/station/files/{item['id']}", method="DELETE") for item in items]
        s["deletesAgain"] = [self.station("delete-again", f"/api/station/files/{item['id']}", method="DELETE") for item in items[:2]]
        s["filesGone"] = [self.station("file-gone", f"/api/station/files/{item['id']}") for item in items[:2]]
        s["inboxEmpty"] = self.station("inbox-empty", "/api/station/inbox")
        # 8. Wrong codes until the address is locked out.
        new_code = self.status_body(s["newCode"])["dropCode"]
        wrong = next(c for c in ("000000", "111111", "222222") if c not in (code, new_code))
        for _ in range(8):
            record = self.drop("drop-lockout", code=wrong, name=stl["name"], data_file=stl_file, content_type="model/stl")
            s["drops"].append(record)
            if record["status"] == 429:
                record["expect"] = {"status": 429, "error": "too_many_attempts", "siteReason": "reasonLocked", "siteStop": True,
                                    "siteLockMinutes": 5}
                break
            record["expect"] = {"status": 401, "error": "wrong_code", "siteReason": "reasonWrongCode", "siteStop": True}
        else:
            raise SystemExit("no lockout after eight wrong codes")
        s["finishedAt"] = utc_now()
        self.down()

    def custom_scenario(self):
        station_name = "\u4e01\u4e01\u6253\u5370\u5ba4 Bambu Studio MD3 on the workshop PC by the window ok"
        env = {"DROP_STATION_NAME": station_name, "DROP_CODE": "4321", "DROP_PUBLIC_URL": "https://Print.Example.test/drop/",
               "DROP_TTL_HOURS": "0.5", "DROP_MAX_BYTES": "1024", "DROP_QUEUE_MAX_FILES": "2", "DROP_QUEUE_MAX_BYTES": "2048"}
        self.begin("custom", env)
        self.up(env)
        s = self.scenario
        s["expect"] = {"stationName": station_name[:60].rstrip(), "dropCode": "4321", "publicUrl": "https://print.example.test/drop",
                       "maxBytes": 1024, "ttlHours": 0.5, "link": "https://print.example.test/drop/#code=4321"}
        s["health"] = self.curl("health", "/healthz")
        s["statusEmpty"] = self.station("status-empty", "/api/station/status")
        s["dropCode"] = "4321"
        samples = {sample["file"]: sample for sample in self.manifest["samples"]}
        with tempfile.TemporaryDirectory(prefix="lan-drop-capture-") as work:
            big = Path(work) / "big.stl"
            big.write_bytes(ascii_stl("big", copies=3))
            assert big.stat().st_size > 1024
            s["drops"] = [
                self.drop("drop-too-large", code="4321", name="big tetra.stl", data_file=big, content_type="model/stl",
                          expect={"status": 413, "error": "too_large", "siteReason": "reasonTooLarge"}),
            ]
        for file_name, sender in (("samples/mesh.obj", "Ada"), ("samples/model.amf", "")):
            sample = samples[file_name]
            s["drops"].append(self.drop("drop", code="4321", name=sample["name"], data_file=self.sample_path(sample), sender=sender,
                                        expect={"status": 201, "sample": file_name, "stationFileName": sample["name"],
                                                "stationSender": sender}))
        housing = samples["samples/housing.step"]
        s["drops"].append(self.drop("drop-queue-full", code="4321", name=housing["name"], data_file=self.sample_path(housing),
                                    expect={"status": 507, "error": "queue_full", "siteReason": "reasonFull", "siteStop": True}))
        s["status"] = self.station("status", "/api/station/status")
        s["inbox"] = self.station("inbox", "/api/station/inbox")
        items = self.status_body(s["inbox"])["items"]
        s["files"] = [self.station("file", f"/api/station/files/{item['id']}") for item in items]
        s["newCode"] = self.station("new-code", "/api/station/drop-code", method="POST")
        s["deletes"] = [self.station("delete", f"/api/station/files/{item['id']}", method="DELETE") for item in items]
        s["inboxEmpty"] = self.station("inbox-empty", "/api/station/inbox")
        s["finishedAt"] = utc_now()
        self.down()

    def refused_settings(self):
        """The service in the real image, started with settings the station could not use, and with
        exactly the station's limits."""
        cases = [
            ({"DROP_MAX_BYTES": "2147483649", "DROP_QUEUE_MAX_BYTES": "4294967296"}, 2),
            ({"DROP_QUEUE_MAX_FILES": "1001"}, 2),
            ({"DROP_PUBLIC_URL": "http://drop_box.lan:8833"}, 2),
            ({"DROP_PUBLIC_URL": "http://print.example.test:0"}, 2),
            ({"DROP_PUBLIC_URL": "https://print.example.test//drop"}, 2),
        ]
        for env, expected in cases:
            flags = []
            for name, value in env.items():
                flags += ["-e", f"{name}={value}"]
            # The image's own command, stopped after 20 seconds should the service start after all.
            run = self.compose("run", "--rm", "--no-deps", "-T", *flags, "lan-model-drop", "timeout", "20", "node", "server/main.mjs",
                               check=False, capture=True)
            message = run.stderr.decode("utf-8").strip().splitlines()
            self.manifest["refusedSettings"].append({"env": env, "exitCode": run.returncode, "expectedExitCode": expected,
                                                     "message": [line for line in message if line.startswith("lan-model-drop:")]})
        limits = {"DROP_MAX_BYTES": "2147483648", "DROP_QUEUE_MAX_FILES": "1000"}
        flags = []
        for name, value in limits.items():
            flags += ["-e", f"{name}={value}"]
        script = ("import('./server/config.mjs').then((m) => { const c = m.readConfig(process.env); "
                  "process.stdout.write(JSON.stringify({ maxBytes: c.maxBytes, queueMaxFiles: c.queueMaxFiles })); })")
        run = self.compose("run", "--rm", "--no-deps", "-T", *flags, "lan-model-drop", "node", "-e", script, check=False, capture=True)
        self.manifest["acceptedLimits"] = {"env": limits, "exitCode": run.returncode, "config": json.loads(run.stdout or b"null")}
        self.down()

    # --------------------------------------------------------------------------------------- run

    def run(self):
        if self.out.exists():
            shutil.rmtree(self.out)
        self.out.mkdir(parents=True)
        with tempfile.TemporaryDirectory(prefix="lan-drop-compose-") as work:
            # Its own image name, so a capture never replaces the image people run.
            self.override = str(Path(work) / "override.yaml")
            Path(self.override).write_text(
                f"services:\n  lan-model-drop:\n    image: localhost/{self.args.project}/lan-model-drop:capture\n", "utf-8")
            self.write_samples()
            dockerfile = (DROP / "Dockerfile").read_text("utf-8")
            self.manifest["baseImage"] = next(line.split()[1] for line in dockerfile.splitlines() if line.startswith("FROM "))
            try:
                self.default_scenario()
                self.custom_scenario()
                self.refused_settings()
            finally:
                self.down()
        manifest = json.dumps(self.manifest, indent=1, ensure_ascii=True) + "\n"
        (self.out / "manifest.json").write_text(manifest, "utf-8")
        # The station key never leaves the container's volume.
        for path in self.out.rglob("*"):
            if path.is_file() and any(key.encode() in path.read_bytes() for key in self.keys):
                raise SystemExit(f"{path} holds the station key")
        print(f"captured {sum(1 for p in self.out.rglob('*') if p.is_file())} files into {self.out}")


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--port", type=int, default=18851)
    parser.add_argument("--project", default="lan-drop-capture")
    parser.add_argument("--out", default=str(REPO / "tests" / "lan_model_drop" / "container_fixtures"))
    parser.add_argument("--chromium", default=None)
    Capture(parser.parse_args()).run()


if __name__ == "__main__":
    main()
