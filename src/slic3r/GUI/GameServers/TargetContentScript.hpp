#pragma once
namespace Slic3r { namespace GUI { namespace GameServers {
inline constexpr char target_content_script[]=R"PYTHON(
import base64,contextlib,fcntl,hashlib,json,os,re,shutil,stat,sys,tempfile,uuid,zipfile
from pathlib import Path,PurePosixPath

def safe_path(root,path):
    resolved=path.resolve()
    if not resolved.is_relative_to(root) or resolved==root:raise ValueError("Content path outside the selected server.")
    return resolved

def read_json(z,name):
    st=z.getinfo(name)
    if st.file_size>1024*1024:raise ValueError("Oversized package descriptor.")
    return json.loads(z.read(st))

def settings(root):
    p=root/"server.properties"
    if p.is_symlink() or p.stat().st_size>1024*1024:raise ValueError("Invalid server properties.")
    return dict(l.split("=",1) for l in p.read_text(encoding="utf8",errors="replace").splitlines() if "=" in l and not l.startswith(("#","!")))

def extract_pack(z,directory):
    size=0
    names=z.infolist()
    if len(names)>100000:raise ValueError("Too many pack entries.")
    for item in names:
        rel=PurePosixPath(item.filename)
        size+=item.file_size
        if (not rel.parts or rel.is_absolute() or ".." in rel.parts or "\\" in item.filename or ":" in item.filename or
                stat.S_ISLNK((item.external_attr>>16)&0xffff) or any(ord(c)<32 for c in item.filename) or size>1024**3):
            raise ValueError("Invalid or oversized pack member.")
        p=directory/rel
        if item.is_dir():p.mkdir(parents=True,exist_ok=True);continue
        p.parent.mkdir(parents=True,exist_ok=True)
        with z.open(item) as src,p.open("xb") as out:shutil.copyfileobj(src,out)

def enable(world,group,pack_id,version):
    path=world/("world_"+group+"_packs.json")
    if not (world/"level.dat").is_file():return "The configured world has not been generated."
    if path.is_symlink():return "The pack list is a symbolic link."
    try:
        raw=path.read_bytes() if path.exists() else b"[]"
        if len(raw)>1024*1024:return "The world pack list exceeds the size limit."
        entries=json.loads(raw)
        if not isinstance(entries,list) or any(not isinstance(e,dict) or not isinstance(e.get("pack_id"),str) for e in entries):
            return "Existing pack registrations are malformed."
        for entry in entries:
            if entry["pack_id"].lower()==pack_id:
                return "" if entry.get("version")==version else "A different version of this pack is already active."
        entries.append({"pack_id":pack_id,"version":version})
        fd,tmp=tempfile.mkstemp(dir=world,prefix=".bambu-pack-list-")
        try:
            with os.fdopen(fd,"w",encoding="utf8") as f:
                if path.exists():os.fchmod(f.fileno(),stat.S_IMODE(path.stat().st_mode))
                json.dump(entries,f,indent=2);f.write("\n");f.flush();os.fsync(f.fileno())
            if (path.read_bytes() if path.exists() else b"[]")!=raw:return "The world pack list changed."
            os.replace(tmp,path)
        finally:
            if os.path.exists(tmp):os.unlink(tmp)
        return ""
    except (OSError,ValueError) as e:return str(e)

def install(root,edition,owner,kind):
    if kind=="ssh":
        if (root/".bambustudio-native").read_text(encoding="ascii")!=owner:raise ValueError("Unrecognized server owner.")
    h=json.loads(sys.stdin.buffer.readline(8193))
    name=h["name"];size=h["size"];sha=h["sha256"];flavour=h["flavour"].lower()
    if (not isinstance(size,int) or not 0<size<=1024**3 or not re.fullmatch("[A-Za-z0-9][A-Za-z0-9_.+ -]{0,155}\.(jar|zip|mcpack)",name,re.I) or
        not re.fullmatch("[0-9a-f]{64}",sha) or edition not in ("java","bedrock")):raise ValueError("Invalid package header.")
    cfg=settings(root);level=cfg.get("level-name","world" if edition=="java" else "Bedrock level")
    if not level or "/" in level or "\\" in level or level in (".",".."):raise ValueError("Invalid world name.")
    with contextlib.ExitStack() as locks:
        file=locks.enter_context((root/".bambustudio-content.lock").open("a+b"));fcntl.flock(file,fcntl.LOCK_EX|fcntl.LOCK_NB)
        if kind=="ssh":
            run=locks.enter_context((root/".bambustudio-run.lock").open("a+b"));fcntl.flock(run,fcntl.LOCK_EX|fcntl.LOCK_NB)
        world=safe_path(root,root/level if edition=="java" else root/"worlds"/level)
        db=world/("session.lock" if edition=="java" else "db/LOCK")
        if db.exists():
            guard=locks.enter_context(db.open("rb"));fcntl.lockf(guard,fcntl.LOCK_SH|fcntl.LOCK_NB)
        work=Path(locks.enter_context(tempfile.TemporaryDirectory(dir=root,prefix=".bambu-install-")))
        source=work/"input";count=0;hash=hashlib.sha256()
        with source.open("xb") as out:
            while count<size:
                line=sys.stdin.buffer.readline(20000)
                chunk=base64.b64decode(line.strip(),validate=True)
                if not chunk or len(chunk)>12288 or count+len(chunk)>size:raise ValueError("Invalid package transfer.")
                hash.update(chunk);out.write(chunk);count+=len(chunk)
        if hash.hexdigest()!=sha:raise ValueError("Transferred package did not match its SHA-256.")
        with zipfile.ZipFile(source) as z:
            names=set(z.namelist())
            if len(names)>200000:raise ValueError("Package inventory exceeds the limit.")
            if edition=="java":
                group=None
                if name.lower().endswith(".jar"):
                    if ((flavour in ("paper","purpur","spigot") and "plugin.yml" in names) or
                        (flavour in ("paper","purpur") and "paper-plugin.yml" in names)):group=root/"plugins"
                    mapping={"fabric":"fabric.mod.json","forge":"META-INF/mods.toml","neoforge":"META-INF/neoforge.mods.toml","quilt":"quilt.mod.json"}
                    if mapping.get(flavour) in names:group=root/"mods"
                elif name.lower().endswith(".zip") and "pack.mcmeta" in names and any(n.startswith("data/") and not n.endswith("/") for n in names):
                    if isinstance(read_json(z,"pack.mcmeta").get("pack"),dict):group=world/"datapacks"
                if group is None:raise ValueError("The package descriptor is incompatible with the selected distribution.")
                folder=safe_path(root,group);folder.mkdir(parents=True,exist_ok=True);os.link(source,folder/name)
                return {"installed":True,"location":str((folder/name).relative_to(root)),"sha256":sha}
            meta=read_json(z,"manifest.json");pack_id=str(uuid.UUID(meta["header"]["uuid"]));version=meta["header"]["version"]
            modules={m["type"] for m in meta["modules"]}
            if not isinstance(version,list) or len(version)!=3 or any(type(n)!=int or not 0<=n<=65535 for n in version):
                raise ValueError("Invalid pack version.")
            group="resource" if modules=={"resources"} else "behavior" if modules and modules<={"data","script"} else ""
            if not group:raise ValueError("Unsupported or mixed Bedrock pack modules.")
            dest=safe_path(root,root/(group+"_packs")/pack_id);dest.parent.mkdir(parents=True,exist_ok=True)
            key=".bambustudio-package-sha256"
            if dest.exists():
                if not (dest/key).is_file() or (dest/key).read_text()!=sha:raise ValueError("An existing pack with that ID was preserved.")
            else:
                staging=work/"contents";staging.mkdir();extract_pack(z,staging)
                with (staging/key).open("x") as f:f.write(sha)
                dest.mkdir()
                try:
                    for child in staging.iterdir():os.rename(child,dest/child.name)
                except BaseException:shutil.rmtree(dest);raise
            detail=enable(world,group,pack_id,version) if h.get("activate") else "Activation was not requested."
            return {"installed":True,"active":not bool(detail),"detail":detail,"location":str(dest.relative_to(root)),"sha256":sha}
try:
    print(json.dumps(install(Path(sys.argv[1]).resolve(strict=True),sys.argv[2],sys.argv[3],sys.argv[4])))
except (OSError,ValueError,KeyError,RuntimeError,TypeError,zipfile.BadZipFile) as e:
    print("Content installation failed: "+str(e),file=sys.stderr);sys.exit(1)
)PYTHON";
}}}
