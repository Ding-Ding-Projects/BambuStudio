"""Execute the streamed-content helper against synthetic archives and disposable target directories."""
import argparse,inspect,pathlib,subprocess,sys

def exercise(code,containers):
    import base64,hashlib,io,json,os,pathlib,subprocess,tempfile,uuid,zipfile
    from contextlib import ExitStack
    def package(entries):
        data=io.BytesIO()
        with zipfile.ZipFile(data,"w") as z:
            for k,v in entries.items():z.writestr(k,v)
        return data.getvalue()
    plug=package({"plugin.yml":"name: fixture","payload":"fixture"})
    uid=str(uuid.uuid4())
    behavior=package({"manifest.json":json.dumps({"header":{"uuid":uid,"version":[1,0,0]},"modules":[{"type":"data"}]}),"functions/test.mcfunction":"say fixture"})
    for container in ([False,True] if containers else [False]):
        for edition in ("java","bedrock"):
            with tempfile.TemporaryDirectory(prefix="bambu-target-content-") as d,ExitStack() as cleanup:
                root=pathlib.Path(d);owner="1"*32
                (root/".bambustudio-native").write_text(owner)
                (root/"server.properties").write_text("level-name=world\n")
                w=root/("world" if edition=="java" else "worlds/world");w.mkdir(parents=True);(w/"level.dat").write_bytes(b"fixture")
                if container:
                    target=subprocess.check_output(["docker","create","--network","none","--mount","type=bind,src="+str(root)+",dst=/data","alpine:3.20","/unused"],text=True).strip()
                    cleanup.callback(lambda:subprocess.run(["docker","rm",target],check=True,stdout=subprocess.DEVNULL))
                    args=["docker","run","--rm","-i","--network","none","--read-only","--cap-drop","ALL","--security-opt","no-new-privileges",
                          "--user",str(os.getuid())+":"+str(os.getgid()),"--volumes-from",target+":rw","python:3.13-alpine","python3","-c",code,"/data",edition,target,"container"]
                else:args=["python3","-c",code,d,edition,owner,"ssh"]
                body=plug if edition=="java" else behavior;name="fixture.jar" if edition=="java" else "fixture.mcpack"
                def send(flavour,sha,bad=False):
                    header={"name":name,"size":len(body),"sha256":sha,"flavour":flavour,"activate":True}
                    lines=[json.dumps(header),*(base64.b64encode(body[i:i+12288]).decode() for i in range(0,len(body),12288))]
                    result=subprocess.run(args,input="\n".join(lines)+"\n",text=True,capture_output=True,timeout=45)
                    if bad:assert result.returncode!=0;return {}
                    assert result.returncode==0,result.stderr
                    return json.loads(result.stdout)
                digest=hashlib.sha256(body).hexdigest()
                send("Paper","0"*64,True)
                if edition=="java":send("Fabric",digest,True)
                receipt=send("Paper",digest);assert receipt["sha256"]==digest and receipt["installed"]
                if edition=="java":
                    assert (root/"plugins"/name).read_bytes()==body
                    send("Paper",digest,True)
                else:
                    assert receipt["active"] and json.loads((w/"world_behavior_packs.json").read_text())[0]["pack_id"]==uid
                    assert send("Paper",digest)["active"]
                print("container" if container else "SSH",edition,"verified streamed package installation and negative checks",flush=True)

if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("--ssh");p.add_argument("--containers",action="store_true");opts=p.parse_args()
    header=(pathlib.Path(__file__).parents[1]/"src/slic3r/GUI/GameServers/TargetContentScript.hpp").read_text(encoding="utf8")
    source=header.split('R"PYTHON(',1)[1].split(')PYTHON"',1)[0]
    if opts.ssh:
        payload=inspect.getsource(exercise)+"\nexercise("+repr(source)+","+repr(opts.containers)+")\n"
        subprocess.run(["ssh","-T","-o","BatchMode=yes","-o","StrictHostKeyChecking=yes","--",opts.ssh,"python3 -"],input=payload,text=True,check=True,timeout=150)
    elif sys.platform!="win32":exercise(source,opts.containers)
    else:print("An explicitly selected Linux host is needed for this target fixture.")
