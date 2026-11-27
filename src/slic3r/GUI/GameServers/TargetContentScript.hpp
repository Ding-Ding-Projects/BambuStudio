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

def content_items(root,edition):
    level=settings(root).get("level-name","world" if edition=="java" else "Bedrock level")
    if not level or "/" in level or "\\" in level or level in (".",".."):raise ValueError("Invalid configured world.")
    groups=["plugins","mods",level+"/datapacks"] if edition=="java" else ["behavior_packs","resource_packs"]
    entries=[]
    for group in groups:
        for state,prefix in [("enabled",""),("disabled",".bambustudio-disabled"),("removed",".bambustudio-removed")]:
            directory=safe_path(root,root/prefix/group)
            if not directory.is_dir():continue
            for p in sorted(directory.iterdir()):
                if p.is_symlink():continue
                if edition=="java":
                    if not p.is_file() or p.suffix.lower()!=(".zip" if group.endswith("datapacks") else ".jar"):continue
                    st=p.stat()
                else:
                    mf=p/"manifest.json"
                    if not p.is_dir() or not mf.is_file() or mf.is_symlink() or not (p/".bambustudio-package-sha256").is_file():continue
                    st=mf.stat()
                key=hashlib.sha256((group+"/"+p.name).encode()).hexdigest()
                entries.append({"id":key,"group":group,"name":p.name,"state":state,"stamp":str(st.st_size)+":"+str(st.st_mtime_ns)})
                if len(entries)>2000:raise ValueError("Content inventory exceeds the supported limit.")
    return entries

def location(root,entry,state):
    prefix={"enabled":"","disabled":".bambustudio-disabled","removed":".bambustudio-removed"}[state]
    return safe_path(root,root/prefix/entry["group"]/entry["name"])

def selected_item(root,edition,h):
    entries=content_items(root,edition)
    matches=[e for e in entries if e["id"]==h.get("id") and e["state"]==h.get("state")]
    if len(matches)!=1 or matches[0]["stamp"]!=h.get("stamp"):raise ValueError("The selected package changed. Refresh its inventory.")
    return matches[0]

def history_folder(root,item):
    p=safe_path(root,root/".bambustudio-content-history"/item["id"])
    p.mkdir(parents=True,exist_ok=True);return p

def replace_owned(root,item,staged):
    prior=location(root,item,item["state"])
    shelf=history_folder(root,item)
    holder=Path(tempfile.mkdtemp(dir=shelf,prefix="revision-"))
    previous=holder/"package"
    os.rename(prior,previous)
    try:
        os.rename(staged,prior)
        fd,tmp=tempfile.mkstemp(dir=shelf,prefix=".pointer-")
        try:
            with os.fdopen(fd,"w",encoding="ascii") as f:f.write(holder.name);f.flush();os.fsync(f.fileno())
            os.replace(tmp,shelf/"previous")
        finally:
            if os.path.exists(tmp):os.unlink(tmp)
    except BaseException:
        if prior.exists():os.rename(prior,staged)
        os.rename(previous,prior);holder.rmdir();raise

def set_registration(root,item,active):
    if not item["group"] in ("behavior_packs","resource_packs"):return
    group=item["group"].split("_")[0]
    path=location(root,item,item["state"])
    meta=json.loads((path/"manifest.json").read_bytes())
    uid=str(uuid.UUID(meta["header"]["uuid"]));version=meta["header"]["version"]
    world_name=settings(root).get("level-name","Bedrock level")
    world=safe_path(root,root/"worlds"/world_name)
    if not (world/"level.dat").is_file():return
    listing=safe_path(root,world/("world_"+group+"_packs.json"))
    if active:
        detail=enable(world,group,uid,version)
        if detail:raise ValueError(detail)
    elif listing.is_file():
        raw=listing.read_bytes()
        if len(raw)>1024*1024:raise ValueError("The world pack list is too large.")
        entries=json.loads(raw)
        if not isinstance(entries,list) or any(not isinstance(x,dict) or not isinstance(x.get("pack_id"),str) for x in entries):raise ValueError("Invalid pack list.")
        keep=[e for e in entries if e["pack_id"].lower()!=uid]
        if len(keep)!=len(entries):
            fd,tmp=tempfile.mkstemp(dir=world,prefix=".pack-list-")
            try:
                with os.fdopen(fd,"w",encoding="utf8") as f:
                    os.fchmod(f.fileno(),stat.S_IMODE(listing.stat().st_mode));json.dump(keep,f,indent=2);f.flush();os.fsync(f.fileno())
                if listing.read_bytes()!=raw:raise ValueError("World pack registrations changed.")
                os.replace(tmp,listing)
            finally:
                if os.path.exists(tmp):os.unlink(tmp)

def registered(root,item):
    if item["group"] not in ("behavior_packs","resource_packs") or item["state"]!="enabled":return False
    level=settings(root).get("level-name","Bedrock level")
    file=safe_path(root,root/"worlds"/level/("world_"+item["group"]))
    file=file.with_name(file.name+".json")
    if not file.is_file():return False
    if file.is_symlink() or file.stat().st_size>1024*1024:raise ValueError("Invalid existing world pack list.")
    entries=json.loads(file.read_bytes())
    if not isinstance(entries,list) or any(not isinstance(e,dict) or not isinstance(e.get("pack_id"),str) for e in entries):raise ValueError("Malformed existing world pack list.")
    return any(e["pack_id"].lower()==item["name"].lower() for e in entries)

def maintain(root,edition,kind,h):
    action=h.get("action")
    if action=="list":return {"packages":content_items(root,edition)}
    if action not in ("enable","disable","remove","rollback"):raise ValueError("Invalid content action.")
    with contextlib.ExitStack() as locks:
        if kind=="ssh":
            lock=locks.enter_context((root/".bambustudio-run.lock").open("a+b"));fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        lock=locks.enter_context((root/".bambustudio-content.lock").open("a+b"));fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        item=selected_item(root,edition,h);src=location(root,item,item["state"]);was_registered=registered(root,item)
        if action=="rollback":
            shelf=history_folder(root,item);name=(shelf/"previous").read_text(encoding="ascii")
            if not re.fullmatch("revision-[a-z0-9_]+",name):raise ValueError("Invalid preserved revision name.")
            previous=safe_path(shelf,shelf/name/"package")
            if previous.is_symlink() or not previous.exists():raise ValueError("The previous revision is unavailable.")
            if was_registered:set_registration(root,item,False)
            try:replace_owned(root,item,previous)
            except BaseException:
                if was_registered:set_registration(root,item,True)
                raise
            if was_registered:set_registration(root,item,True)
            return {"changed":True,"state":item["state"],"action":action}
        new={"enable":"enabled","disable":"disabled","remove":"removed"}[action]
        if new==item["state"]:raise ValueError("The package already has that state.")
        dest=location(root,item,new);dest.parent.mkdir(parents=True,exist_ok=True)
        if dest.exists() or dest.is_symlink():raise ValueError("An existing destination was preserved.")
        if was_registered:set_registration(root,item,False)
        try:os.rename(src,dest)
        except BaseException:
            if was_registered:set_registration(root,item,True)
            raise
        if edition=="bedrock" and new=="enabled":
            try:set_registration(root,dict(item,state=new),True)
            except BaseException:os.rename(dest,src);raise
        return {"changed":True,"state":new,"action":action}

def install(root,edition,owner,kind):
    if kind=="ssh":
        if (root/".bambustudio-native").read_text(encoding="ascii")!=owner:raise ValueError("Unrecognized server owner.")
    h=json.loads(sys.stdin.buffer.readline(8193))
    if h.get("action","install") not in ("install","update"):return maintain(root,edition,kind,h)
    updating=h.get("action")=="update"
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
        item=selected_item(root,edition,h) if updating else None
        if updating and item["state"]=="removed":raise ValueError("Restore the removed package before updating it.")
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
                folder=safe_path(root,group);folder.mkdir(parents=True,exist_ok=True)
                if updating:
                    if item["group"]!=folder.relative_to(root).as_posix():raise ValueError("The replacement type differs from the selected package.")
                    replace_owned(root,item,source);result=location(root,item,item["state"])
                else:
                    result=folder/name;os.link(source,result)
                return {"installed":True,"updated":updating,"location":str(result.relative_to(root)),"sha256":sha}
            meta=read_json(z,"manifest.json");pack_id=str(uuid.UUID(meta["header"]["uuid"]));version=meta["header"]["version"]
            modules={m["type"] for m in meta["modules"]}
            if not isinstance(version,list) or len(version)!=3 or any(type(n)!=int or not 0<=n<=65535 for n in version):
                raise ValueError("Invalid pack version.")
            group="resource" if modules=={"resources"} else "behavior" if modules and modules<={"data","script"} else ""
            if not group:raise ValueError("Unsupported or mixed Bedrock pack modules.")
            dest=safe_path(root,root/(group+"_packs")/pack_id);dest.parent.mkdir(parents=True,exist_ok=True)
            key=".bambustudio-package-sha256"
            if updating:
                if item["group"]!=group+"_packs" or item["name"].lower()!=pack_id:raise ValueError("The replacement pack UUID or module group differs.")
                staging=work/"contents";staging.mkdir();extract_pack(z,staging)
                with (staging/key).open("x") as f:f.write(sha)
                was_registered=registered(root,item)
                if was_registered:set_registration(root,item,False)
                try:replace_owned(root,item,staging)
                except BaseException:
                    if was_registered:set_registration(root,item,True)
                    raise
                dest=location(root,item,item["state"])
                if was_registered:set_registration(root,item,True)
                return {"installed":True,"updated":True,"state":item["state"],"location":str(dest.relative_to(root)),"sha256":sha}
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
