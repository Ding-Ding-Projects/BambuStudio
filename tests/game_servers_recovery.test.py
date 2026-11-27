"""Exercise the production offline-world recovery script on disposable, explicitly selected targets."""
import argparse
import json
import os
import pathlib
import subprocess
import sys
import shutil
import tempfile
import zipfile

def probe(code, containers):
    import json,os,pathlib,subprocess,tempfile,zipfile
    from contextlib import ExitStack
    def call(argv, bad=False):
        p=subprocess.run(argv,text=True,capture_output=True,timeout=60)
        if bad:
            assert p.returncode!=0,"Expected a rejected action"
            return {}
        assert p.returncode==0,p.stderr
        return json.loads(p.stdout.strip().splitlines()[-1])
    for edition in ("java","bedrock"):
        with tempfile.TemporaryDirectory(prefix="bambu-recovery-") as d, ExitStack() as cleanup:
            tmp=pathlib.Path(d);root=tmp/"source";root.mkdir()
            w=root/("world" if edition=="java" else "worlds/Bedrock level")
            w.mkdir(parents=True);(w/"level.dat").write_bytes(b"synthetic world metadata")
            (w/"test.dat").write_bytes(bytes(range(256))*16)
            (root/"server.properties").write_text("level-name="+w.name+"\nrcon.password=disposable-fixture\nscript-debugger-passcode=disposable-fixture\n")
            owner="a"*32;(root/".bambustudio-native").write_text(owner)
            def ssh_args(mode,archive="",dst=""):
                return ["python3","-c",code,mode,"ssh",str(root),edition,owner,archive,dst]
            output=call(ssh_args("snapshot"));archive=output["archive"]
            assert archive in call(ssh_args("list"))["archives"]
            out=tmp/"restore";receipt=call(ssh_args("restore",archive,str(out)))
            assert receipt["files"]==3
            assert (out/w.relative_to(root)/"test.dat").read_bytes()==(w/"test.dat").read_bytes()
            assert "disposable-fixture" not in (out/"server.properties").read_text()
            call(ssh_args("restore",archive,str(out)),True)
            bad=ssh_args("list");bad[-3]="0"*32;call(bad,True)
            if containers:
                stop=subprocess.check_output(["docker","create","--network","none","--mount","type=bind,src="+str(root)+",dst=/data,readonly","alpine:3.20","/unused"],text=True).strip()
                cleanup.callback(lambda c=stop:subprocess.run(["docker","rm",c],check=True,stdout=subprocess.DEVNULL))
                bk=tmp/"store";re=tmp/"restores";bk.mkdir();re.mkdir()
                args=["docker","run","--rm","--network","none","--read-only","--cap-drop","ALL","--security-opt","no-new-privileges",
                      "--volumes-from",stop+":ro","--mount","type=bind,src="+str(bk)+",dst=/backups","--mount","type=bind,src="+str(re)+",dst=/restore"]
                # Use the test identity's UID for the bind-mounted temporary output. Production named volumes are initialized by the helper.
                args+=["--user",str(os.getuid())+":"+str(os.getgid()),"python:3.13-alpine","python3","-c",code]
                def container_args(mode,archive="",dst=""):return args+[mode,"container","/data",edition,stop,archive,dst]
                snap=call(container_args("snapshot"))
                assert snap["archive"] in call(container_args("list"))["archives"]
                assert call(container_args("restore",snap["archive"],"/restore/verified"))["files"]==3
                assert (re/"verified"/w.relative_to(root)/"level.dat").read_bytes()==(w/"level.dat").read_bytes()
                call(container_args("restore",snap["archive"],"/restore/verified"),True)
            print(edition,"snapshot, archive list, isolated restore, readback, protected-setting exclusions and occupied-destination rejection passed",flush=True)

if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("--ssh",help="Explicitly authorized OpenSSH host for isolated fixtures")
    ap.add_argument("--containers",action="store_true",help="Also exercise a disposable Docker engine fixture")
    options=ap.parse_args()
    header=(pathlib.Path(__file__).parents[1]/"src/slic3r/GUI/GameServers/WorldRecoveryScript.hpp").read_text(encoding="utf-8")
    source=header.split('R"PYTHON(',1)[1].split(')PYTHON"',1)[0]
    if options.ssh:
        import inspect
        payload=inspect.getsource(probe)+"\nprobe("+repr(source)+","+repr(options.containers)+")\n"
        subprocess.run(["ssh","-T","-o","BatchMode=yes","-o","StrictHostKeyChecking=yes","-o","ConnectTimeout=8","--",options.ssh,"python3 -"],input=payload,text=True,check=True,timeout=180)
    elif os.name!="nt":
        probe(source,options.containers)
    else:
        print("Native POSIX world-lock verification needs an explicitly selected Linux fixture host.")
