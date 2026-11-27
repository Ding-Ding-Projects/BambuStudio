#pragma once
namespace Slic3r { namespace GUI { namespace GameServers {
inline constexpr char target_settings_script[]=R"PYTHON(
import fcntl,json,os,re,stat,sys,tempfile,hashlib
from pathlib import Path

def safe_key(key):
    return isinstance(key,str) and re.fullmatch("[a-z0-9][a-z0-9._-]{0,95}",key) and not any(
        word in key for word in ("password","passcode","token","secret","private-key","api-key"))

def run(root,owner,ssh,request):
    if ssh and (root/".bambustudio-native").read_text(encoding="ascii")!=owner:
        raise ValueError("Server ownership is unverified.")
    mode=request["mode"];allowed=set(request["keys"])
    if len(allowed)>512 or not all(safe_key(k) for k in allowed):raise ValueError("Invalid setting inventory.")
    file=root/"server.properties"
    if file.is_symlink():raise ValueError("Linked settings files are not editable.")
    if mode=="identity":
        s=file.stat();return {"user":str(s.st_uid)+":"+str(s.st_gid)}
    with file.open("rb") as stream:
        info=os.fstat(stream.fileno());raw=stream.read(1024*1024+1)
    if len(raw)>1024*1024:raise ValueError("Settings exceed the supported size.")
    revision=hashlib.sha256(raw).hexdigest()
    rows={};lines=raw.splitlines(keepends=True)
    for pos,line in enumerate(lines):
        parts=line.rstrip(b"\r\n").split(b"=",1)
        if len(parts)!=2:continue
        key=parts[0].decode("ascii",errors="ignore")
        if key not in allowed:continue
        if key in rows:rows[key]["duplicate"]=True;continue
        val=parts[1].decode("utf-8",errors="replace")
        if len(val)>1024 or any(ord(c)<32 for c in val):continue
        rows[key]={"key":key,"value":val,"position":pos,"duplicate":False}
    if mode=="read":
        return {"revision":revision,"settings":[{"key":k,"value":v["value"],"duplicate":v["duplicate"]} for k,v in rows.items()]}
    key=request["key"];value=request["value"]
    if mode!="write" or key not in rows or rows[key]["duplicate"] or not isinstance(value,str) or len(value.encode())>1024 or any(ord(c)<32 for c in value):
        raise ValueError("The requested setting or value is invalid.")
    if request["revision"]!=revision:raise ValueError("The settings changed; reload first.")
    if os.geteuid()!=info.st_uid:raise ValueError("The current identity does not own this settings file.")
    with (root/".bambustudio-settings.lock").open("a+b") as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        if ssh:
            session=(root/".bambustudio-run.lock").open("a+b")
            fcntl.flock(session,fcntl.LOCK_EX|fcntl.LOCK_NB)
        pos=rows[key]["position"];prior=lines[pos]
        ending=prior[len(prior.rstrip(b"\r\n")):]
        lines[pos]=key.encode()+b"="+value.encode()+ending
        fd,tmp=tempfile.mkstemp(prefix=".bambu-properties-",dir=root)
        try:
            with os.fdopen(fd,"wb") as out:
                os.fchmod(out.fileno(),stat.S_IMODE(info.st_mode))
                if os.fstat(out.fileno()).st_gid!=info.st_gid:os.fchown(out.fileno(),-1,info.st_gid)
                out.writelines(lines);out.flush();os.fsync(out.fileno())
            if file.is_symlink() or hashlib.sha256(file.read_bytes()).hexdigest()!=revision:raise ValueError("The external settings changed; reload first.")
            os.replace(tmp,file)
        finally:
            if os.path.exists(tmp):os.unlink(tmp)
    return {"saved":True}

try:
    target,owner,kind=sys.argv[1:4]
    request=json.loads(sys.stdin.buffer.readline(65537))
    result=run(Path(target).resolve(strict=True),owner,kind=="ssh",request)
    print(json.dumps(result,separators=(",",":")))
except (ValueError,RuntimeError,OSError,TypeError,KeyError) as error:
    print(str(error),file=sys.stderr);sys.exit(1)
)PYTHON";
}}}
