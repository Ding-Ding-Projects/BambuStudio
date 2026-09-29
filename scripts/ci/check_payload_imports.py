#!/usr/bin/env python3
"""Refuse a Windows payload whose executables import a DLL the payload lacks.

    py -3 scripts/ci/check_payload_imports.py <payload-dir> [<file> ...]

Reads the import and delay-import tables of each file (default:
bambu-studio.exe and BambuStudio.dll) and of every DLL they pull in from the
payload, and fails when an imported DLL is neither in the payload nor a
Windows system DLL. The Visual C++ runtime counts as part of the payload, not
of Windows: a machine without the VC++ redistributable has none of it, so it
must ship beside the app.

Release md3-v143 shipped BambuStudio.dll without its OpenCascade, FFmpeg and
GMP DLLs and without the Visual C++ runtime: LoadLibrary failed with error 126
and the launcher exited with -1 before the app could log anything. The Ninja
build had skipped the DLL copy that the install step ships.
"""
from __future__ import annotations

import os
import struct
import sys

VC_RUNTIME_PREFIXES = ('msvcp140', 'vcruntime140', 'concrt140', 'vccorlib140', 'vcomp140')
# Loaded with LoadLibrary rather than imported, so no import table names them:
# without WebView2Loader.dll every embedded web page shows "Embedded Browser
# Unavailable".
RUNTIME_LOADED = ('WebView2Loader.dll',)


def pe_imports(path: str) -> list[str]:
    with open(path, 'rb') as fh:
        data = fh.read()
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise ValueError(f'{path}: not a PE file')
    sections = struct.unpack_from('<H', data, pe + 6)[0]
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    optional = pe + 24
    magic = struct.unpack_from('<H', data, optional)[0]
    directories = optional + (112 if magic == 0x20B else 96)
    table = [struct.unpack_from('<8sIIII', data, optional + optional_size + i * 40) for i in range(sections)]

    def offset(rva):
        for _name, vsize, va, rsize, raw in table:
            if va <= rva < va + max(vsize, rsize):
                return raw + rva - va
        return None

    names = []
    for index, step, name_field in ((1, 20, 12), (13, 32, 4)):  # imports, delay imports
        rva, _size = struct.unpack_from('<II', data, directories + index * 8)
        if not rva:
            continue
        pos = offset(rva)
        while pos is not None and any(data[pos:pos + step]):
            name_rva = struct.unpack_from('<I', data, pos + name_field)[0]
            start = offset(name_rva)
            names.append(data[start:data.index(b'\0', start)].decode('ascii', 'replace'))
            pos += step
    return names


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 2
    payload = argv[1]
    roots = argv[2:] or ['bambu-studio.exe', 'BambuStudio.dll']
    system32 = os.path.join(os.environ.get('SystemRoot', r'C:\Windows'), 'System32')
    present = {name.lower(): name for name in os.listdir(payload)}
    missing: dict[str, set[str]] = {}
    seen: set[str] = set()
    queue = [name for name in roots]
    while queue:
        current = queue.pop()
        if current.lower() in seen:
            continue
        seen.add(current.lower())
        path = os.path.join(payload, present.get(current.lower(), current))
        if not os.path.isfile(path):
            missing.setdefault(current, set()).add('(entry point)')
            continue
        for dll in pe_imports(path):
            key = dll.lower()
            if key.startswith(('api-ms-', 'ext-ms-')):
                continue
            if key in present:
                queue.append(present[key])
                continue
            is_vc_runtime = key.startswith(VC_RUNTIME_PREFIXES)
            if not is_vc_runtime and os.path.isfile(os.path.join(system32, dll)):
                continue
            missing.setdefault(dll, set()).add(current)
    for dll in RUNTIME_LOADED:
        if dll.lower() not in present:
            missing.setdefault(dll, set()).add('(loaded at run time)')
    checked = len(seen)
    if missing:
        print(f'payload {payload}: {len(missing)} imported DLL(s) missing after checking {checked} file(s):')
        for dll, users in sorted(missing.items(), key=lambda item: item[0].lower()):
            print(f'  {dll}  (imported by {", ".join(sorted(users))})')
        return 1
    print(f'payload {payload}: every import of {checked} file(s) resolves in the payload or in Windows')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
