"""Check persistent process identity and reconnect inside an explicitly selected disposable tmux container."""
import inspect,pathlib,subprocess,sys

def exercise(code):
    import json,os,pathlib,subprocess,tempfile,time
    with tempfile.TemporaryDirectory() as d:
        root=pathlib.Path(d);owner="9"*32
        (root/".bambustudio-native").write_text(owner);(root/"server.properties").write_text("server-name=fixture\n")
        server=root/"bedrock_server"
        server.write_text("#!/bin/sh\necho ready\nwhile IFS= read -r line;do case \"$line\" in say*) echo yes > warned;; list) echo fixture-list;echo yes > checked;; stop) echo fixture-stop;exit 0;; esac;done\n")
        server.chmod(0o700)
        def run(op,input="",who=owner,bad=False,extra=()):
            r=subprocess.run(["python3","-c",code,d,who,"bedrock",op,*extra],input=input,text=True,capture_output=True,timeout=12)
            if bad:assert r.returncode!=0,(op,r.stdout);return r
            assert r.returncode==0,(op,r.stderr,r.stdout);return r
        run("start")
        first=json.loads((root/".bambustudio-session.json").read_text())
        assert "is running" in run("inspect").stdout
        run("attach","list\n")
        for _ in range(30):
            if (root/"checked").exists():break
            time.sleep(.05)
        assert (root/"checked").exists(),"command did not reach reattached process"
        assert json.loads((root/".bambustudio-session.json").read_text())==first
        run("start",bad=True);run("attach",who="0"*32,bad=True)
        record=root/".bambustudio-session.json"
        tampered=dict(first,start="0");record.write_text(json.dumps(tampered));run("inspect",bad=True);record.write_text(json.dumps(first))
        run("warn",extra=("2",))
        for _ in range(30):
            if (root/"warned").exists():break
            time.sleep(.05)
        assert (root/"warned").exists(),"scheduled warning missing"
        run("restart")
        second=json.loads(record.read_text())
        assert second["pid"]!=first["pid"] and "is running" in run("inspect").stdout
        run("attach","stop\n");time.sleep(.4)
        assert "exited with code 0" in run("inspect").stdout
        run("restart",bad=True)
        print("tmux: warning, graceful replacement, stopped-target skip, detach, verified same PID/start/reconnect, console command, duplicate launch refusal, wrong owner, stale identity and graceful exit passed.")
if __name__=="__main__":
    if len(sys.argv)!=2:raise SystemExit("Pass an explicitly selected disposable Linux container with tmux and Python.")
    h=(pathlib.Path(__file__).parents[1]/"src/slic3r/GUI/GameServers/ManagedSshSession.hpp").read_text(encoding="utf8")
    code=h.split('R"PYTHON(',1)[1].split(')PYTHON"',1)[0]
    p=inspect.getsource(exercise)+"\nexercise("+repr(code)+")\n"
    subprocess.run(["docker","exec","-i",sys.argv[1],"python3","-"],input=p,text=True,timeout=60,check=True)
