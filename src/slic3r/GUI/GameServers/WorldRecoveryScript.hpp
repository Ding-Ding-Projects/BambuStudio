#pragma once
namespace Slic3r { namespace GUI { namespace GameServers {
inline constexpr char world_recovery_script[]=R"PYTHON(
import contextlib, datetime, fcntl, hashlib, json, os, re, shutil, sys, tempfile, zipfile
from pathlib import Path, PurePosixPath

def sha256_file(path):
    h=hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda:f.read(1024*1024),b""):
            h.update(chunk)
    return h.hexdigest()

def property_values(path):
    return dict(line.split("=",1) for line in path.read_text(encoding="utf-8",errors="replace").splitlines()
                if "=" in line and not line.startswith(("#","!")))

def safe_member(name):
    p=PurePosixPath(name)
    return (bool(p.parts) and not p.is_absolute() and ".." not in p.parts and "\\" not in name and
            ":" not in name and not any(ord(c)<32 for c in name))

def config_bytes(path):
    blocked={"rcon.password","script-debugger-passcode"}
    return b"".join(line for line in path.read_bytes().splitlines(keepends=True)
                    if line.split(b"=",1)[0].strip().decode("ascii","ignore").lower() not in blocked)

def snapshot(root,store,edition,owner):
    props=root/"server.properties"
    if not props.is_file() or props.is_symlink():raise ValueError("The server has no regular server.properties file.")
    if props.stat().st_size>1024*1024:raise ValueError("The properties file exceeds the size limit.")
    data=property_values(props);world=data.get("level-name","world" if edition=="java" else "Bedrock level")
    if not world or len(PurePosixPath(world).parts)!=1 or world in (".","..") or not safe_member(world):
        raise ValueError("Invalid configured world directory.")
    roots=[world,world+"_nether",world+"_the_end"] if edition=="java" else ["worlds","behavior_packs","resource_packs"]
    worlds=[root/x for x in roots if (root/x/"level.dat").is_file()] if edition=="java" else list((root/"worlds").glob("*/level.dat"))
    if not worlds:raise ValueError("No generated world found for this server.")
    with contextlib.ExitStack() as locks:
        # A live JVM or LevelDB holds a write lock. Test the source without altering its lock file.
        for p in worlds:
            w=p if p.is_dir() else p.parent
            lock=w/("session.lock" if edition=="java" else "db/LOCK")
            if lock.exists():
                f=locks.enter_context(lock.open("rb"))
                fcntl.lockf(f,fcntl.LOCK_SH|fcntl.LOCK_NB)
        paths=[props]
        for key in roots:
            top=root/key
            if not top.exists():continue
            if top.is_symlink():raise ValueError("World or pack root is a symbolic link.")
            for directory,dirs,files in os.walk(top,followlinks=False):
                parent=Path(directory)
                if any((parent/d).is_symlink() for d in dirs):raise ValueError("Linked world content requires manual backup.")
                for name in files:
                    if name in ("session.lock","LOCK"):continue
                    p=parent/name
                    if p.is_symlink() or not p.is_file():raise ValueError("Linked or special world content requires manual backup.")
                    paths.append(p)
        for name in ("whitelist.json","ops.json","banned-players.json","banned-ips.json","allowlist.json","permissions.json"):
            p=root/name
            if p.is_file() and not p.is_symlink():paths.append(p)
        if len(paths)>1000000:raise ValueError("The snapshot exceeds the file-count limit.")
        size=sum(p.stat().st_size for p in paths)
        if size>128*1024**3 or shutil.disk_usage(store).free<min(size+64*1024**2,128*1024**3):
            raise ValueError("Insufficient free space for a conservative snapshot estimate.")
        stamp=datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
        final=store/("world-"+stamp+".zip")
        fd,tmp=tempfile.mkstemp(dir=store,prefix=".pending-");os.close(fd)
        try:
            records={}
            with zipfile.ZipFile(tmp,"w",zipfile.ZIP_DEFLATED,compresslevel=1,allowZip64=True) as z:
                for p in paths:
                    name=p.relative_to(root).as_posix()
                    if not safe_member(name) or name in records:raise ValueError("Invalid backup member.")
                    before=p.stat()
                    h=hashlib.sha256();length=0
                    with z.open(name,"w",force_zip64=True) as out:
                        if p==props:
                            content=config_bytes(p);out.write(content);h.update(content);length=len(content)
                        else:
                            with p.open("rb") as src:
                                for chunk in iter(lambda:src.read(1024*1024),b""):
                                    out.write(chunk);h.update(chunk);length+=len(chunk)
                    after=p.stat()
                    if before.st_ino!=after.st_ino or before.st_mtime_ns!=after.st_mtime_ns or before.st_size!=after.st_size:
                        raise ValueError("A source file changed while the snapshot was written.")
                    records[name]=[length,h.hexdigest()]
                z.writestr("BAMBU-WORLD-BACKUP.json",json.dumps({"schema":1,"owner":owner,"edition":edition,"files":records}))
            with zipfile.ZipFile(tmp) as z:
                if z.testzip() is not None:raise ValueError("Archive CRC verification failed.")
            fingerprint=sha256_file(Path(tmp));os.link(tmp,final)
            return {"archive":final.name,"files":len(records),"sha256":fingerprint}
        finally:
            os.unlink(tmp)

def restore(store,archive,dest,owner):
    if not re.fullmatch(r"world-[0-9]{8}T[0-9]{12}Z\.zip",archive):raise ValueError("Unknown archive name.")
    path=store/archive
    if path.is_symlink() or not path.is_file():raise ValueError("The archive is unavailable.")
    if not dest.is_absolute() or dest.name in ("",".",".."):raise ValueError("Choose a new absolute destination directory.")
    dest=dest.parent.resolve(strict=True)/dest.name
    with zipfile.ZipFile(path) as z:
        meta=z.getinfo("BAMBU-WORLD-BACKUP.json")
        if meta.file_size>128*1024**2:raise ValueError("Oversized snapshot manifest.")
        manifest=json.loads(z.read(meta))
        if manifest.get("schema")!=1 or manifest.get("owner")!=owner:raise ValueError("Archive ownership does not match.")
        files=manifest["files"]
        if (not isinstance(files,dict) or len(files)>1000000 or len(z.infolist())!=len(files)+1 or
            set(z.namelist())!=set(files)|{"BAMBU-WORLD-BACKUP.json"}):raise ValueError("Archive inventory mismatch.")
        expanded=sum(z.getinfo(n).file_size for n in files)
        if expanded>128*1024**3 or shutil.disk_usage(dest.parent).free<expanded+64*1024**2:raise ValueError("Insufficient restore space.")
        dest.mkdir(mode=0o700);identity=dest.stat()
        try:
            for name,(size,sha) in files.items():
                if not safe_member(name) or size!=z.getinfo(name).file_size:raise ValueError("Invalid restore member.")
                out=dest/PurePosixPath(name);out.parent.mkdir(parents=True,exist_ok=True)
                with z.open(name) as stream,out.open("xb") as f:shutil.copyfileobj(stream,f)
                if sha256_file(out)!=sha:raise ValueError("Restore file readback mismatch.")
            return {"restored":str(dest),"files":len(files),"bytes":expanded}
        except BaseException:
            now=dest.lstat()
            if now.st_ino==identity.st_ino and now.st_dev==identity.st_dev:shutil.rmtree(dest)
            raise

try:
    mode,transport,source,edition,owner,archive,destination=sys.argv[1:8]
    if not re.fullmatch("[a-f0-9]{32}|[a-f0-9]{64}",owner) or edition not in ("java","bedrock"):raise ValueError("Invalid target.")
    root=Path(source).resolve(strict=True)
    with contextlib.ExitStack() as stack:
        if transport=="ssh":
            if (root/".bambustudio-native").read_text(encoding="ascii")!=owner:raise ValueError("Server ownership changed.")
            store=root.parent/(".bambustudio-backups-"+owner)
            if mode=="snapshot":
                lock=stack.enter_context((root/".bambustudio-run.lock").open("a+b"))
                fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        elif transport=="container" and source=="/data":store=Path("/backups")
        else:raise ValueError("Invalid transport.")
        if store.is_symlink():raise ValueError("Backup store cannot be a symbolic link.")
        if mode!="list":store.mkdir(mode=0o700,exist_ok=True)
        if mode=="list":result={"archives":sorted((p.name for p in store.glob("world-*.zip") if not p.is_symlink()),reverse=True)[:100]}
        elif mode=="snapshot":result=snapshot(root,store,edition,owner)
        elif mode=="restore":
            dest=Path(destination)
            if dest.resolve().is_relative_to(root) or dest.resolve().is_relative_to(store):raise ValueError("Restore must use a separate path.")
            result=restore(store,archive,dest,owner)
        else:raise ValueError("Invalid recovery action.")
    print(json.dumps(result))
except (OSError,ValueError,RuntimeError,KeyError,zipfile.BadZipFile) as error:
    print("Recovery failed: "+str(error),file=sys.stderr);sys.exit(1)
)PYTHON";
}}}
