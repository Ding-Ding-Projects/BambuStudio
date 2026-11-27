#pragma once
namespace Slic3r { namespace GUI { namespace GameServers {
// The SSH host runs this fixed installer using its existing Python 3 interpreter.
// User choices are passed as quoted arguments, never interpolated into the script.
inline constexpr char remote_server_setup[] = R"PYTHON(
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import sys
import zipfile

def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def provision(package, destination, edition, port, owner):
    if not package.is_absolute() or not destination.is_absolute() or destination.name in ("", ".", ".."):
        raise ValueError("Use absolute source and destination paths.")
    if not 1 <= port <= 65535 or edition not in ("java", "bedrock"):
        raise ValueError("Invalid port or edition.")
    if len(owner) != 32 or any(c not in "0123456789abcdef" for c in owner):
        raise ValueError("Invalid ownership identifier.")
    if not package.is_file() or package.stat().st_size > 1024**3:
        raise ValueError("The selected package is unavailable or exceeds 1 GiB.")
    destination = destination.parent.resolve(strict=True) / destination.name
    source_hash = digest(package)
    with zipfile.ZipFile(package) as z:
        if edition == "java":
            manifest = z.getinfo("META-INF/MANIFEST.MF")
            if manifest.file_size > 1024**2 or b"main-class:" not in z.read(manifest).lower():
                raise ValueError("Choose a runnable Java server JAR.")
        else:
            z.getinfo("bedrock_server")
            z.getinfo("server.properties")
            if len(z.infolist()) > 100000 or sum(i.file_size for i in z.infolist()) > 4 * 1024**3:
                raise ValueError("The archive exceeds the extraction limits.")
        destination.mkdir(mode=0o700)
        identity = destination.stat()
        try:
            if edition == "java":
                output = destination / "server.jar"
                shutil.copyfile(package, output)
                if digest(output) != source_hash:
                    raise ValueError("Server JAR readback did not match.")
                (destination / "eula.txt").write_text("eula=false\n", encoding="utf-8")
                (destination / "server.properties").write_text(
                    "server-ip=127.0.0.1\nserver-port=" + str(port) + "\nonline-mode=true\n", encoding="utf-8")
            else:
                for item in z.infolist():
                    entry = PurePosixPath(item.filename)
                    mode = (item.external_attr >> 16) & 0xffff
                    if (entry.is_absolute() or ".." in entry.parts or "\\" in item.filename or not entry.parts or
                            stat.S_ISLNK(mode) or any(ord(c)<32 for c in item.filename)):
                        raise ValueError("Invalid archive member.")
                    target = destination.joinpath(*entry.parts)
                    if item.is_dir():
                        target.mkdir(parents=True, exist_ok=True)
                        continue
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with z.open(item) as src, target.open("xb") as dst:
                        shutil.copyfileobj(src, dst)
                (destination / "bedrock_server").chmod(0o700)
                props = destination / "server.properties"
                lines = [line for line in props.read_text(encoding="utf-8-sig").splitlines() if line.partition("=")[0].strip() != "server-port"]
                props.write_text("\n".join(lines + ["server-port=" + str(port)]) + "\n", encoding="utf-8")
            with (destination / ".bambustudio-native").open("x", encoding="ascii") as f:
                f.write(owner)
            print(json.dumps({"edition":edition,"port":port,"sha256":source_hash,"owner":owner}))
        except BaseException:
            now = destination.lstat()
            if now.st_ino == identity.st_ino and now.st_dev == identity.st_dev:
                shutil.rmtree(destination)
            raise

try:
    provision(Path(sys.argv[1]),Path(sys.argv[2]),sys.argv[3],int(sys.argv[4]),sys.argv[5])
except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
    print("Provisioning failed: " + str(error), file=sys.stderr)
    sys.exit(1)
)PYTHON";
}}}
