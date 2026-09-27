"""Identify a packaged process without borrowing an unrelated desktop window."""
from __future__ import annotations

import json
import ntpath
import re
import subprocess
from datetime import datetime, timezone


_QUERY = r'''Get-CimInstance Win32_Process -Filter "Name = 'bambu-studio.exe'" |
    Select-Object ProcessId,ParentProcessId,ExecutablePath,CommandLine,@{Name='CreationDate';Expression={$_.CreationDate.ToUniversalTime().ToString('o')}} |
    ConvertTo-Json -Compress -Depth 3'''


def process_snapshot() -> list[dict]:
    completed = subprocess.run(
        ["powershell", "-NoProfile", "-NonInteractive", "-Command", _QUERY],
        capture_output=True, text=True, timeout=20, check=False,
    )
    if completed.returncode:
        raise RuntimeError("Win32_Process discovery failed")
    if not completed.stdout.strip():
        return []
    data = json.loads(completed.stdout)
    return data if isinstance(data, list) else [data]


def _created_at(value: str) -> datetime | None:
    try:
        result = datetime.fromisoformat(value.replace("Z", "+00:00"))
        return result.astimezone(timezone.utc)
    except (ValueError, TypeError, AttributeError):
        return None


def owned_process_inventory(processes: list[dict], *, exe: str, datadir: str,
                            launched_at: datetime, launch_pid: int) -> list[dict]:
    """Identify live original and direct-child processes independently of HWNDs."""
    if launched_at is None or launch_pid is None:
        return []
    # The launch command contains the spelling supplied by the caller. A
    # hosted temp directory may be a junction, so Path.resolve() can change
    # that spelling even when the command still addresses the same profile.
    exe_path = ntpath.normcase(ntpath.abspath(exe))
    profile_path = ntpath.normcase(ntpath.abspath(datadir))
    owned = []
    for process in processes:
        try:
            pid = int(process["ProcessId"])
            image = ntpath.normcase(ntpath.abspath(str(process["ExecutablePath"])))
            command = str(process["CommandLine"] or "")
            created = _created_at(process.get("CreationDate"))
        except (KeyError, TypeError, ValueError, OSError):
            continue
        if image != exe_path or created is None:
            continue
        if created < launched_at:
            continue
        # Even the original PID must retain the isolated profile in its command line.
        profile_args = re.findall(r'(?:^|\s)--datadir\s+"([^"]+)"(?=\s|$)',
                                  command, flags=re.IGNORECASE)
        if len(profile_args) != 1 or ntpath.normcase(ntpath.abspath(profile_args[0])) != profile_path:
            continue
        try:
            parent = int(process.get("ParentProcessId") or 0)
        except (TypeError, ValueError):
            continue
        if pid != launch_pid and parent != launch_pid:
            continue
        owned.append({"pid": pid, "parent_pid": parent,
                      "created_at_utc": created.isoformat(), "launch_pid": pid == launch_pid})
    return sorted(owned, key=lambda item: (not item["launch_pid"], item["pid"]))


def owned_processes(processes: list[dict], *, exe: str, datadir: str,
                    launched_at: datetime, launch_pid: int,
                    desktop_pids: set[int]) -> list[int]:
    """Return only owned processes that also have HWNDs on the named desktop."""
    inventory = owned_process_inventory(processes, exe=exe, datadir=datadir,
                                        launched_at=launched_at, launch_pid=launch_pid)
    return [item["pid"] for item in inventory if item["pid"] in desktop_pids]
