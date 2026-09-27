"""Identify a packaged process without borrowing an unrelated desktop window."""
from __future__ import annotations

import json
import os
import subprocess
from datetime import datetime, timezone
from pathlib import Path


_QUERY = r'''Get-CimInstance Win32_Process -Filter "Name = 'bambu-studio.exe'" |
    Select-Object ProcessId,ParentProcessId,ExecutablePath,CommandLine,CreationDate |
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


def owned_processes(processes: list[dict], *, exe: str, datadir: str,
                    launched_at: datetime, launch_pid: int,
                    desktop_pids: set[int]) -> list[int]:
    """Require path, profile, launch time, and a window on our named desktop."""
    exe_path = os.path.normcase(os.path.normpath(str(Path(exe).resolve())))
    profile_path = os.path.normcase(os.path.normpath(str(Path(datadir).resolve())))
    owned = []
    for process in processes:
        try:
            pid = int(process["ProcessId"])
            image = os.path.normcase(os.path.normpath(str(Path(process["ExecutablePath"]).resolve())))
            command = str(process["CommandLine"] or "")
            created = _created_at(process.get("CreationDate"))
        except (KeyError, TypeError, ValueError, OSError):
            continue
        if image != exe_path or created is None:
            continue
        if created < launched_at:
            continue
        # Even the original PID must retain the isolated profile in its command line.
        normalized_command = os.path.normcase(command.replace("/", "\\"))
        if f'--datadir "{profile_path}"' not in normalized_command:
            continue
        if pid != launch_pid and (pid not in desktop_pids or int(process.get("ParentProcessId") or 0) != launch_pid):
            continue
        owned.append(pid)
    return sorted(set(owned), key=lambda pid: (pid != launch_pid, pid))
