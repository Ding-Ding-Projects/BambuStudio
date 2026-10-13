#!/usr/bin/env python3
"""Bounded local startup diagnostics. Validation alone never launches the target."""
from __future__ import annotations
import argparse
import ctypes
from ctypes import wintypes as wt
from datetime import datetime, timezone, timedelta
import importlib.util
import json
import mmap
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import time
import uuid

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("local_review", HERE / "local-native-review.py")
review = importlib.util.module_from_spec(spec)
spec.loader.exec_module(review)
require, regular, digest = review.require, review.regular, review.digest
LaunchFailure = review.LaunchFailure
MAX_OUTPUT = 8 * 1024 * 1024
TRACE_SECONDS = 140
WORKER_SECONDS = 225
TOOL_SECONDS = 270
TOOLS = ("cdb.exe", "symchk.exe", "dbgeng.dll", "dbghelp.dll", "symsrv.dll")


def pdb_identity(path):
    regular(path)
    with path.open("rb") as stream:
        header = stream.read(56)
        require(len(header) == 56 and header[:32] == b"Microsoft C/C++ MSF 7.00\r\n\x1aDS\0\0\0", "Unsupported PDB container")
        block, _, count, size, _, address = struct.unpack_from("<6I", header, 32)
        require(block in (512, 1024, 2048, 4096) and count * block == path.stat().st_size
                and 0 < size <= 16 * 1024 * 1024 and address < count, "Invalid PDB bounds")
        pages = (size + block - 1) // block
        require(pages * 4 <= block, "Unsupported PDB directory size")
        stream.seek(address * block)
        ids = struct.unpack("<" + str(pages) + "I", stream.read(pages * 4))
        directory = bytearray()
        for index in ids:
            require(index < count, "PDB directory block outside file")
            stream.seek(index * block)
            directory.extend(stream.read(block))
        directory = directory[:size]
        number = struct.unpack_from("<I", directory)[0]
        require(2 <= number <= 100000 and 4 + 4 * number <= size, "Invalid PDB stream count")
        sizes = struct.unpack_from("<" + str(number) + "I", directory, 4)
        cursor = 4 + 4 * number
        for index, length in enumerate(sizes[:2]):
            pages = 0 if length == 0xffffffff else (length + block - 1) // block
            require(cursor + pages * 4 <= size, "Invalid PDB stream directory")
            blocks = struct.unpack_from("<" + str(pages) + "I", directory, cursor)
            cursor += pages * 4
            if index == 1:
                require(length >= 28 and blocks and blocks[0] < count, "PDB identity missing")
                stream.seek(blocks[0] * block)
                info = stream.read(28)
                return {"guid": str(uuid.UUID(bytes_le=info[12:28])), "age": struct.unpack_from("<I", info, 8)[0]}


def pe_identity(path):
    regular(path)
    with path.open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
        def u32(offset):
            require(0 <= offset <= len(data) - 4, "PE read outside file")
            return struct.unpack_from("<I", data, offset)[0]
        require(data[:2] == b"MZ", "Not a PE image")
        pe = u32(60)
        require(data[pe:pe+4] == b"PE\0\0" and pe + 24 <= len(data), "Invalid PE header")
        machine, sections = struct.unpack_from("<HH", data, pe + 4)
        optional = struct.unpack_from("<H", data, pe + 20)[0]
        start = pe + 24
        require(machine == 0x8664 and optional >= 168 and struct.unpack_from("<H", data, start)[0] == 0x20b,
                "Expected an x64 PE32+ image")
        section_start = start + optional
        require(sections <= 96 and section_start + sections * 40 <= len(data), "Invalid PE section table")
        def offset(rva):
            for i in range(sections):
                row = section_start + i * 40
                virtual_size, virtual, raw_size, raw = struct.unpack_from("<4I", data, row + 8)
                if virtual <= rva < virtual + max(virtual_size, raw_size):
                    require(rva - virtual < raw_size, "PE RVA lacks file bytes")
                    return raw + rva - virtual
            raise ValueError("PE RVA not mapped")
        rva, size = struct.unpack_from("<II", data, start + 112 + 6 * 8)
        require(size and size <= 28 * 1024 and size % 28 == 0, "PE debug directory unavailable")
        begin = offset(rva)
        require(begin + size <= len(data), "PE debug directory outside file")
        records = []
        for pos in range(begin, begin + size, 28):
            kind, length, _, pointer = struct.unpack_from("<4I", data, pos + 12)
            if kind == 2:
                require(25 <= length <= 4096 and pointer + length <= len(data), "Invalid CodeView bounds")
                entry = data[pointer:pointer+length]
                require(entry[:4] == b"RSDS" and b"\0" in entry[24:], "Unsupported CodeView record")
                name = entry[24:].split(b"\0", 1)[0].decode("utf-8", "strict").replace("\\", "/").rsplit("/", 1)[-1]
                require(re.fullmatch(r"%?[A-Za-z0-9_.-]+\.pdb%?", name), "Unsafe embedded PDB name")
                records.append({"guid": str(uuid.UUID(bytes_le=entry[4:20])), "age": struct.unpack_from("<I", entry, 20)[0], "name": name})
        require(len(records) == 1, "PE CodeView identity is ambiguous")
        return records[0]


def validate_symbols(exe, paths):
    result = []
    for image, pdb in zip((exe, exe.parent / "BambuStudio.dll"), paths):
        identity = pe_identity(image)
        require(pdb_identity(pdb) == {k: identity[k] for k in ("guid", "age")}, "Target PDB does not match image")
        result.append({"image": image.name, "pdb": str(pdb), "sha256": digest(pdb), **identity})
    require(len(result) == 2, "Both target PDBs are required")
    return result


def validate_tools(cdb, manifest):
    rows = review.read_json(manifest)
    require(isinstance(rows, list) and len(rows) == len(TOOLS), "Debugger identity receipt is incomplete")
    indexed = {row["Name"]: row for row in rows}
    require(set(indexed) == set(TOOLS) and cdb.name.lower() == "cdb.exe", "Debugger tool names differ")
    for name in TOOLS:
        row = indexed[name]
        require(row.get("Signature") == "Valid" and row.get("Machine") == "0x8664"
                and "Microsoft Corporation" in row.get("Publisher", "")
                and digest(cdb.parent / name) == row.get("SHA256", "").lower(), "Debugger tool identity differs")
    return {name: indexed[name]["SHA256"].lower() for name in TOOLS}


def commands():
    return ('.echo LOCAL_TRACE_INITIAL\n.printf "LOCAL_TRACE_TARGET %u %u\\n", @$tpid, @$tid\n'
            '.symopt- 0x40\n.symopt+ 0x400\n.reload /f ntdll.dll\nlmv m ntdll\n'
            '!gflag +sls\n!gflag\n'
            'sxe -c ".echo LOCAL_TRACE_EXCEPTION; .lastevent; k 24; gn" av\n'
            'sxe -c ".echo LOCAL_TRACE_EXIT; .lastevent; q" epr\n'
            '.echo LOCAL_TRACE_READY\n')


def acknowledgement(text):
    targets = re.findall(r"(?m)^LOCAL_TRACE_TARGET ([1-9]\d*) ([1-9]\d*)\s*$", text)
    if (text.splitlines().count("LOCAL_TRACE_INITIAL") == 1 and text.splitlines().count("LOCAL_TRACE_READY") == 1
            and len(targets) == 1 and re.search(r"(?im)^\s*sls\s*-\s*Show loader snaps\s*$", text)
            and re.search(r"(?im)^.*\bntdll\s+\(pdb symbols\)", text)
            and not re.search(r"(?i)mismatched|could not be loaded|unable to verify", text)):
        return tuple(map(int, targets[0]))
    return None


def isolated_environment(root):
    temp = root / "temp"
    temp.mkdir()
    environment = {k: v for k, v in os.environ.items() if k.upper() not in
                   {"TEMP", "TMP", "_NT_SYMBOL_PATH", "_NT_ALT_SYMBOL_PATH", "_NT_DEBUGGER_EXTENSION_PATH"}
                   and not k.upper().startswith(("BAMBU_AUTOMATION", "BAMBU_LAYOUT_PROBE"))}
    environment.update(TEMP=str(temp), TMP=str(temp))
    return environment


class NativeDebugger(review.NativeSession):
    """The reviewed atomic job pattern, extended only with restricted CDB pipes."""
    def __init__(self, cdb, arguments, environment, cwd):
        import msvcrt
        read_in, write_in = os.pipe()
        read_out, write_out = os.pipe()
        self.fds = {read_in, write_in, read_out, write_out}
        self.stdin = self.stdout = None
        try:
            os.set_inheritable(read_in, True)
            os.set_inheritable(write_out, True)
            self.pipe_handles = (msvcrt.get_osfhandle(read_in), msvcrt.get_osfhandle(write_out), msvcrt.get_osfhandle(write_out))
            self._start(cdb, arguments, environment, cwd)
            for fd in (read_in, write_out):
                os.close(fd)
                self.fds.remove(fd)
            self.stdin = os.fdopen(write_in, "wb", buffering=0)
            self.fds.remove(write_in)
            self.stdout = os.fdopen(read_out, "rb", buffering=0)
            self.fds.remove(read_out)
        except Exception:
            for fd in self.fds:
                os.close(fd)
            if getattr(self, "job", None):
                self.close()
            raise

    def _start(self, cdb, command_arguments, environment_values, cwd):
        require(os.name == "nt", "Native review requires Windows")
        from ctypes import wintypes as w
        self.k = ctypes.WinDLL("kernel32", use_last_error=True)
        self.u = ctypes.WinDLL("user32", use_last_error=True)
        class SI(ctypes.Structure):
            _fields_ = [("cb", w.DWORD), ("reserved", w.LPWSTR), ("desktop", w.LPWSTR),
                ("title", w.LPWSTR), *[(n, w.DWORD) for n in ("x", "y", "cx", "cy", "cols", "rows", "fill", "flags")],
                ("show", w.WORD), ("reserved2size", w.WORD), ("reserved2", ctypes.c_void_p),
                ("stdin", w.HANDLE), ("stdout", w.HANDLE), ("stderr", w.HANDLE)]
        class PI(ctypes.Structure):
            _fields_ = [("process", w.HANDLE), ("thread", w.HANDLE), ("pid", w.DWORD), ("tid", w.DWORD)]
        class SIEX(ctypes.Structure):
            _fields_ = [("startup", SI), ("attributes", ctypes.c_void_p)]
        class Basic(ctypes.Structure):
            _fields_ = [("processTime", ctypes.c_int64), ("jobTime", ctypes.c_int64), ("flags", w.DWORD),
                ("minWorking", ctypes.c_size_t), ("maxWorking", ctypes.c_size_t), ("activeLimit", w.DWORD),
                ("affinity", ctypes.c_size_t), ("priority", w.DWORD), ("scheduling", w.DWORD)]
        class Extended(ctypes.Structure):
            _fields_ = [("basic", Basic), ("io", ctypes.c_uint64 * 6),
                *[(n, ctypes.c_size_t) for n in ("processMemory", "jobMemory", "peakProcess", "peakJob")]]
        signatures = {"CreateJobObjectW": ([ctypes.c_void_p, w.LPCWSTR], w.HANDLE),
            "SetInformationJobObject": ([w.HANDLE, ctypes.c_int, ctypes.c_void_p, w.DWORD], w.BOOL),
            "CreateProcessW": ([w.LPCWSTR, w.LPWSTR, ctypes.c_void_p, ctypes.c_void_p, w.BOOL, w.DWORD,
                ctypes.c_void_p, w.LPCWSTR, ctypes.POINTER(SI), ctypes.POINTER(PI)], w.BOOL),
            "InitializeProcThreadAttributeList": ([ctypes.c_void_p, w.DWORD, w.DWORD,
                ctypes.POINTER(ctypes.c_size_t)], w.BOOL),
            "UpdateProcThreadAttribute": ([ctypes.c_void_p, w.DWORD, ctypes.c_size_t,
                ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_void_p], w.BOOL),
            "DeleteProcThreadAttributeList": ([ctypes.c_void_p], None),
            "ResumeThread": ([w.HANDLE], w.DWORD), "TerminateProcess": ([w.HANDLE, w.UINT], w.BOOL),
            "TerminateJobObject": ([w.HANDLE, w.UINT], w.BOOL),
            "WaitForSingleObject": ([w.HANDLE, w.DWORD], w.DWORD),
            "GetExitCodeProcess": ([w.HANDLE, ctypes.POINTER(w.DWORD)], w.BOOL),
            "QueryInformationJobObject": ([w.HANDLE, ctypes.c_int, ctypes.c_void_p, w.DWORD, ctypes.c_void_p], w.BOOL),
            "CloseHandle": ([w.HANDLE], w.BOOL)}
        for name, (arguments, result) in signatures.items():
            getattr(self.k, name).argtypes, getattr(self.k, name).restype = arguments, result
        self.u.OpenInputDesktop.argtypes, self.u.OpenInputDesktop.restype = [w.DWORD, w.BOOL, w.DWORD], w.HANDLE
        self.u.GetUserObjectInformationW.argtypes = [w.HANDLE, ctypes.c_int, ctypes.c_void_p, w.DWORD, ctypes.POINTER(w.DWORD)]
        self.u.CloseDesktop.argtypes = [w.HANDLE]
        desktop = self.u.OpenInputDesktop(0, False, 1)
        require(desktop, "Input desktop is unavailable")
        name, length = ctypes.create_unicode_buffer(256), w.DWORD()
        try:
            require(self.u.GetUserObjectInformationW(desktop, 2, name, ctypes.sizeof(name), ctypes.byref(length))
                    and name.value.casefold() == "default", "Visible default desktop is not active")
        finally:
            self.u.CloseDesktop(desktop)
        self.job, self.process = self.k.CreateJobObjectW(None, None), None
        require(self.job, "Cannot create owned process job")
        info = Extended()
        info.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        try:
            require(self.k.SetInformationJobObject(self.job, 9, ctypes.byref(info), ctypes.sizeof(info)), "Cannot contain process")
            env = dict(environment_values)
            for key in list(env):
                if key.upper().startswith("BAMBU_AUTOMATION") or key.upper().startswith("BAMBU_LAYOUT_PROBE"):
                    del env[key]
            environment = ctypes.create_unicode_buffer("\0".join(k + "=" + v for k, v in sorted(env.items())) + "\0\0")
            startup, process = SIEX(), PI()
            startup.startup.cb = ctypes.sizeof(SIEX)
            startup.startup.desktop, startup.startup.flags, startup.startup.show = "WinSta0\\Default", 0x101, 0
            startup.startup.stdin, startup.startup.stdout, startup.startup.stderr = self.pipe_handles
            command = ctypes.create_unicode_buffer(subprocess.list2cmdline(command_arguments))
            size = ctypes.c_size_t()
            self.k.InitializeProcThreadAttributeList(None, 2, 0, ctypes.byref(size))
            require(0 < size.value <= 1024 * 1024, "Cannot size process attributes")
            attributes = ctypes.create_string_buffer(size.value)
            require(self.k.InitializeProcThreadAttributeList(attributes, 2, 0, ctypes.byref(size)),
                    "Cannot initialize process attributes")
            try:
                jobs = (w.HANDLE * 1)(self.job)
                # PROC_THREAD_ATTRIBUTE_JOB_LIST (WinBase.h: input attribute 13).
                # Membership is established by CreateProcessW itself. Even an
                # abrupt worker exit before that call returns closes the job and
                # terminates the child. Never fall back to post-creation assignment.
                require(self.k.UpdateProcThreadAttribute(attributes, 0, 0x2000D, jobs,
                            ctypes.sizeof(jobs), None, None), "Cannot bind creation-time process job")
                handles = (w.HANDLE * 2)(self.pipe_handles[0], self.pipe_handles[1])
                require(self.k.UpdateProcThreadAttribute(attributes, 0, 0x20002, handles,
                            ctypes.sizeof(handles), None, None), "Cannot limit inherited pipe handles")
                startup.attributes = ctypes.cast(attributes, ctypes.c_void_p)
                # EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED
                require(self.k.CreateProcessW(str(cdb), command, None, None, True, 0x8080404, environment,
                            str(cwd), ctypes.cast(ctypes.byref(startup), ctypes.POINTER(SI)),
                            ctypes.byref(process)), "Native launch failed")
            finally:
                self.k.DeleteProcThreadAttributeList(attributes)
            self.process, self.pid = process.process, int(process.pid)
            try:
                require(self.k.ResumeThread(process.thread) != 0xFFFFFFFF, "Process resume failed")
            except Exception:
                self.k.TerminateProcess(self.process, 1)
                raise
            finally:
                self.k.CloseHandle(process.thread)
        except Exception as exc:
            try:
                teardown = "verified" if self.close() else "unverified"
            except Exception:
                teardown = "unverified"
            raise LaunchFailure(str(exc), teardown) from exc


    def retain_target(self, pid, tid, exe):
        k, u = self.k, self.u
        definitions = {
            "OpenProcess": ([wt.DWORD, wt.BOOL, wt.DWORD], wt.HANDLE),
            "OpenThread": ([wt.DWORD, wt.BOOL, wt.DWORD], wt.HANDLE),
            "GetProcessIdOfThread": ([wt.HANDLE], wt.DWORD),
            "QueryFullProcessImageNameW": ([wt.HANDLE, wt.DWORD, wt.LPWSTR, ctypes.POINTER(wt.DWORD)], wt.BOOL),
            "IsProcessInJob": ([wt.HANDLE, wt.HANDLE, ctypes.POINTER(wt.BOOL)], wt.BOOL)}
        for name, (arguments, result) in definitions.items():
            getattr(k, name).argtypes, getattr(k, name).restype = arguments, result
        u.GetThreadDesktop.argtypes, u.GetThreadDesktop.restype = [wt.DWORD], wt.HANDLE
        handle = k.OpenProcess(0x101000, False, pid)  # query limited + synchronize
        require(handle, "Debug target handle unavailable")
        try:
            for item in (handle, self.process):
                yes = wt.BOOL()
                require(k.IsProcessInJob(item, self.job, ctypes.byref(yes)) and yes.value,
                        "Debug target or debugger is outside owned job")
            path, size = ctypes.create_unicode_buffer(32768), wt.DWORD(32768)
            require(k.QueryFullProcessImageNameW(handle, 0, path, ctypes.byref(size))
                    and Path(path.value).resolve() == exe.resolve(), "Debug target image differs")
            require(k.WaitForSingleObject(handle, 0) == 258, "Debug target already exited")
            thread = k.OpenThread(0x800, False, tid)
            require(thread, "Debug target thread unavailable")
            try:
                require(k.GetProcessIdOfThread(thread) == pid, "Debug thread belongs to another process")
                desktop = u.GetThreadDesktop(tid)
                name, needed = ctypes.create_unicode_buffer(256), wt.DWORD()
                require(desktop and u.GetUserObjectInformationW(desktop, 2, name, ctypes.sizeof(name), ctypes.byref(needed))
                        and name.value.casefold() == "default", "Debug target desktop differs")
            finally:
                k.CloseHandle(thread)
            target = review.NativeSession.__new__(review.NativeSession)
            target.process, target.pid, target.k = handle, pid, k
            return target
        except Exception:
            k.CloseHandle(handle)
            raise


def arguments(cdb, exe, profile, script, symbols, cache):
    for path in (cdb, exe, profile, script, symbols, cache):
        require(path.is_absolute() and not re.search(r'["\r\n*;]', str(path)), "Unsafe debugger path")
    return [str(cdb), "-sins", "-ses", "-y", str(symbols) + ";srv*" + str(cache)
            + "*https://msdl.microsoft.com/download/symbols", "-cf", str(script), str(exe), "--datadir", str(profile)]


def readiness(args):
    receipt = review.read_json(args.build_receipt)
    exe = review.validate_receipt(receipt, args.producer, args.source_commit)
    tools = validate_tools(args.cdb, args.tool_identity)
    symbols = validate_symbols(exe, (args.exe_pdb, args.dll_pdb))
    return exe, tools, symbols


def trace(args, root, factory=NativeDebugger):
    report = {"schemaVersion": 1, "kind": "local-startup-diagnostic", "sourceCommit": args.source_commit,
              "executionClass": "instrumented-from-creation", "desktop": "visible",
              "launch": "not_attempted", "loaderTrace": "not_attempted", "continuation": "not_attempted",
              "targetExit": {"status": "not_observed"}, "teardown": "not_attempted",
              "probe": "not_attempted", "screenshot": "not_attempted", "runtimeAcceptance": "unverified"}
    session = target = reader = None
    data, lock, overflow = bytearray(), threading.Lock(), threading.Event()
    try:
        exe, tools, symbols = readiness(args)
        report.update(buildReceiptSha256=digest(args.build_receipt), debuggerFiles=tools,
                      symbols=symbols, driverSha256=digest(Path(__file__).resolve()),
                      provenanceDriverSha256=digest(HERE / "local-native-review.py"))
        profile = review.seed_profile(root)
        environment = isolated_environment(root)
        local, cache = root / "symbols", root / "symbol-cache"
        local.mkdir()
        cache.mkdir()
        for row in symbols:
            destination = local / row["name"]
            shutil.copyfile(row["pdb"], destination)
            require(digest(destination) == row["sha256"], "Staged target PDB changed")
        script = root / "initial.txt"
        script.write_text(commands(), encoding="ascii")
        readiness(args)  # Inputs are checked again immediately before creation.
        report["launch"] = "failed"
        session = factory(args.cdb, arguments(args.cdb, exe, profile, script, local, cache), environment, exe.parent)
        report.update(launch="started", debuggerPid=session.pid, loaderTrace="incomplete")
        def drain():
            try:
                while True:
                    block = session.stdout.read(4096)
                    if not block:
                        break
                    with lock:
                        if len(data) + len(block) > MAX_OUTPUT:
                            overflow.set()
                            break
                        data.extend(block)
            except Exception:
                overflow.set()
        reader = threading.Thread(target=drain, daemon=True)
        reader.start()
        deadline = time.monotonic() + TRACE_SECONDS
        while time.monotonic() < deadline:
            require(not overflow.is_set(), "Debugger output exceeded bound or failed")
            with lock:
                text = bytes(data).decode("utf-8", errors="replace")
            observed = acknowledgement(text)
            if target is None and observed:
                target = session.retain_target(*observed, exe)
                report.update(targetPid=target.pid, ownership="verified", continuation="pending")
                session.stdin.write(b"g\n")
                session.stdin.flush()
                report["continuation"] = "sent"
            if not session.alive():
                break
            time.sleep(0.1)
        require(not session.alive(), "Debugger trace deadline reached")
        reader.join(timeout=3)
        require(not reader.is_alive() and not overflow.is_set(), "Debugger output is incomplete")
        report["debuggerExit"] = session.observe_target_exit()
        with lock:
            text = bytes(data).decode("utf-8", errors="replace")
        require(target is not None and "LOCAL_TRACE_EXIT" in [line.strip() for line in text.splitlines()],
                "Debugger target exit marker missing")
        report["loaderTrace"] = "collected"
        readiness(args)
    except Exception as exc:
        report["failure"] = str(exc)
        if isinstance(exc, LaunchFailure):
            report["teardown"] = exc.teardown
    finally:
        if target is not None:
            try:
                report["targetExit"] = target.observe_target_exit()
            except Exception:
                report["targetExit"] = {"status": "unavailable", "observedBeforeTeardown": True}
        if session is not None:
            try:
                report["teardown"] = "verified" if session.close() else "unverified"
            except Exception:
                report["teardown"] = "unverified"
            if target is not None:
                session.k.CloseHandle(target.process)
            if session.stdin:
                session.stdin.close()
            if reader:
                reader.join(timeout=3)
            if session.stdout and (not reader or not reader.is_alive()):
                session.stdout.close()
        with lock:
            raw = bytes(data)
        report["outputTruncated"] = overflow.is_set() or bool(reader and reader.is_alive())
        if raw:
            path = root / "cdb-loader.log"
            path.write_bytes(raw)
            report["restrictedLog"] = {"file": path.name, "sha256": digest(path), "bytes": len(raw)}
        launcher = root / "temp" / "bbs-launcher-trace.log"
        if launcher.is_file():
            report["isolatedLauncherLog"] = {"file": "temp/bbs-launcher-trace.log", "sha256": digest(launcher)}
        (root / "trace.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report


PATH_ARGS = ("producer", "build_receipt", "lowlevel_cli", "cdb", "tool_identity", "exe_pdb", "dll_pdb")


def worker(request):
    root = request.parent
    require(root.parent.resolve() == Path(tempfile.gettempdir()).resolve()
            and root.name.startswith("bambu-startup-trace-") and request.name == "request.json", "Unowned worker request")
    data = review.read_json(request)
    require(data.get("execute") is True and data.get("desktop") == "visible", "Explicit visible execution missing")
    require(timedelta(0) <= datetime.now(timezone.utc) - review.timestamp(data["requestedAtUtc"]) <= timedelta(seconds=30),
            "Stale diagnostic request")
    require(not (root / "stop").exists(), "Diagnostic stopped before creation")
    with (root / "worker.claim").open("x", encoding="ascii") as claim:
        claim.write(str(os.getpid()))
    args = argparse.Namespace(source_commit=data["source_commit"], **{name: Path(data[name]) for name in PATH_ARGS})
    finished = threading.Event()
    def watchdog():
        deadline = time.monotonic() + WORKER_SECONDS
        while not finished.wait(0.1):
            if (root / "stop").exists() or time.monotonic() >= deadline:
                os._exit(124)  # Only this worker's atomic, nonbreakaway job loses its owner.
    threading.Thread(target=watchdog, daemon=True).start()
    try:
        result = trace(args, root)
        return 0 if "failure" not in result and result["teardown"] == "verified" else 1
    finally:
        finished.set()


def worker_command(request):
    values = (sys.executable, str(Path(__file__).resolve()), str(request))
    require(all(not re.search(r'["\r\n&|<>^%!]', value) for value in values), "Unsafe worker shell path")
    return f'"{values[0]}" "{values[1]}" --worker "{values[2]}"'


def dispatch(args, root, call=review.cheap):
    request = root / "request.json"
    data = {name: str(getattr(args, name)) for name in (*PATH_ARGS, "source_commit")}
    data.update(execute=True, desktop="visible", requestedAtUtc=datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"))
    request.write_text(json.dumps(data), encoding="utf-8")
    try:
        result = call(args.lowlevel_cli, "run_command", command=worker_command(request), shell=True, cwd=str(HERE), timeout=TOOL_SECONDS)
        require(result.get("timed_out") is False and type(result.get("returncode")) is int, "Diagnostic worker did not terminate")
        report = review.read_json(root / "trace.json", 1024 * 1024)
        require(report.get("kind") == "local-startup-diagnostic" and report.get("sourceCommit") == args.source_commit
                and report.get("teardown") in ("verified", "unverified", "not_attempted"), "Worker report identity differs")
        report["workerExitCode"] = result["returncode"]
        return report
    except BaseException:
        (root / "stop").write_text("stop", encoding="ascii")
        report = {"launch": "unverified", "loaderTrace": "unverified", "teardown": "unverified",
                  "targetExit": {"status": "unavailable"}, "runtimeAcceptance": "unverified",
                  "failure": "Worker interrupted, timed out or lacked valid evidence"}
        (root / "wrapper-failure.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in PATH_ARGS:
        parser.add_argument("--" + name.replace("_", "-"), type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--desktop", choices=("visible",))
    args = parser.parse_args()
    exe, tools, symbols = readiness(args)
    regular(args.lowlevel_cli)
    if not args.execute:
        require(args.desktop is None, "Desktop selection requires execution")
        print(json.dumps({"provenance": "validated", "symbols": symbols, "debuggerFiles": tools, "launch": "not_attempted"}))
        return 0
    require(os.name == "nt" and args.desktop == "visible", "Explicit visible Windows desktop required")
    root = Path(tempfile.mkdtemp(prefix="bambu-startup-trace-"))
    report = dispatch(args, root)
    print(json.dumps({"evidenceDirectory": str(root), **{key: report[key] for key in
                     ("launch", "loaderTrace", "targetExit", "teardown", "runtimeAcceptance")}}))
    return 0 if "failure" not in report and report.get("workerExitCode") == 0 and report["teardown"] == "verified" else 1


if __name__ == "__main__":
    try:
        raise SystemExit(worker(Path(sys.argv[2])) if len(sys.argv) == 3 and sys.argv[1] == "--worker" else main())
    except (ValueError, KeyError, OSError, struct.error, subprocess.SubprocessError) as exc:
        print(json.dumps({"status": "blocked", "reason": str(exc), "launch": "not_attempted"}), file=sys.stderr)
        raise SystemExit(2)
