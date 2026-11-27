"""Verify the owned-target configuration protocol on disposable files only."""
import argparse,inspect,pathlib,subprocess,sys

def exercise(code,containers):
    import hashlib,json,os,pathlib,subprocess,tempfile
    from contextlib import ExitStack
    with tempfile.TemporaryDirectory(prefix="bambu-settings-") as d,ExitStack() as cleanup:
        root=pathlib.Path(d);owner="b"*32;(root/".bambustudio-native").write_text(owner)
        sample=b"# existing\r\nmax-players=10\r\nrcon.password=only-a-fixture\r\nopaque=unrelated-value\r\n"
        file=root/"server.properties"
        target=[("ssh",["python3","-c",code,str(root),owner,"ssh"])]
        if containers:
            ref=subprocess.check_output(["docker","create","--network","none","--mount","type=bind,src="+str(root)+",dst=/data","alpine:3.20","/unused"],text=True).strip()
            cleanup.callback(lambda:subprocess.run(["docker","rm",ref],check=True,stdout=subprocess.DEVNULL))
            target.append(("container",["docker","run","--rm","-i","--network","none","--read-only","--cap-drop","ALL","--security-opt","no-new-privileges",
                "--user",str(os.getuid())+":"+str(os.getgid()),"--volumes-from",ref+":rw","python:3.13-alpine","python3","-c",code,"/data",ref,"container"]))
        for kind,args in target:
            file.write_bytes(sample);file.chmod(0o640)
            def send(request,bad=False):
                result=subprocess.run(args,input=json.dumps(request)+"\n",text=True,capture_output=True,timeout=45)
                if bad:assert result.returncode!=0;return {}
                assert result.returncode==0,result.stderr
                assert "only-a-fixture" not in result.stdout and "unrelated-value" not in result.stdout
                return json.loads(result.stdout)
            read=send({"mode":"read","keys":["max-players"]})
            assert read["settings"]==[{"key":"max-players","value":"10","duplicate":False}]
            send({"mode":"read","keys":["rcon.password"]},True)
            request={"mode":"write","revision":read["revision"],"key":"max-players","value":"12","keys":["max-players"]}
            assert send(request)=={"saved":True}
            assert file.read_bytes()==sample.replace(b"players=10",b"players=12") and file.stat().st_mode&0o777==0o640
            send(request,True)
            print(kind,"read, privacy filtering, typed edit transport, mode and unrelated-byte preservation, stale-write rejection passed",flush=True)

if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("--ssh");p.add_argument("--containers",action="store_true");opts=p.parse_args()
    h=(pathlib.Path(__file__).parents[1]/"src/slic3r/GUI/GameServers/TargetSettingsScript.hpp").read_text(encoding="utf8")
    source=h.split('R"PYTHON(',1)[1].split(')PYTHON"',1)[0]
    if opts.ssh:
        payload=inspect.getsource(exercise)+"\nexercise("+repr(source)+","+repr(opts.containers)+")\n"
        subprocess.run(["ssh","-T","-o","BatchMode=yes","-o","StrictHostKeyChecking=yes","--",opts.ssh,"python3 -"],input=payload,text=True,check=True,timeout=120)
    elif sys.platform!="win32":exercise(source,opts.containers)
    else:print("An explicitly selected Linux target is needed for target-side POSIX settings.")
