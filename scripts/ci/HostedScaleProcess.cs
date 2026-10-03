// Used only by Invoke-HostedDisplayScale.ps1 on disposable hosted Windows jobs.
using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Security.Principal;
using System.Text.RegularExpressions;
using Microsoft.Win32.SafeHandles;

public static class HostedScaleProcess
{
    // Fixed diagnostic stages only. Never include exception text or child data.
    public enum Stage { Initial, Job, Limits, Streams, Attributes, Command, Create,
        Assign, Reader, Resume, Observe, Timeout, OutputLimit, ExitQuery,
        Drain, DrainTimeout, DrainRejected, Decode, Complete, InvalidName }
    public sealed class Result { public bool Terminated; public int Code = -1; public string Output = "";
        public Stage ProcessStage; public int? NativeError; }
    [StructLayout(LayoutKind.Sequential)] struct SA { public int Length; public IntPtr Descriptor; public int Inherit; }
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct SI {
        public int Size; public string Reserved, Desktop, Title;
        public int X,Y,XSize,YSize,XChars,YChars,Fill,Flags;
        public short Show, ReservedSize; public IntPtr Reserved2, Input, Output, Error;
    }
    [StructLayout(LayoutKind.Sequential)] struct PI { public IntPtr Process, Thread; public uint Pid, Tid; }
    [StructLayout(LayoutKind.Sequential)] struct SIX { public SI Start; public IntPtr Attributes; }
    [StructLayout(LayoutKind.Sequential)] struct Limits {
        public long ProcessTime, JobTime; public uint Flags;
        public UIntPtr MinSet, MaxSet; public uint Active; public UIntPtr Affinity; public uint Priority, Scheduling;
    }
    [StructLayout(LayoutKind.Sequential)] struct IO { public ulong A,B,C,D,E,F; }
    [StructLayout(LayoutKind.Sequential)] struct Extended {
        public Limits Basic; public IO Io; public UIntPtr ProcessMemory, JobMemory, PeakProcess, PeakJob;
    }
    [StructLayout(LayoutKind.Sequential)] struct Accounting {
        public long A,B,C,D; public uint Faults, Total, Active, Terminated;
    }
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] static extern IntPtr CreateJobObject(IntPtr a,string n);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern IntPtr CreateJobObject(ref SA a,string n);
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool ConvertStringSecurityDescriptorToSecurityDescriptor(string text,uint revision,out IntPtr descriptor,out uint size);
    [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr memory);
    [DllImport("kernel32.dll")] static extern bool SetInformationJobObject(IntPtr j,int c,ref Extended e,uint n);
    [DllImport("kernel32.dll")] static extern bool QueryInformationJobObject(IntPtr j,int c,out Accounting a,uint n,IntPtr r);
    [DllImport("kernel32.dll")] static extern bool AssignProcessToJobObject(IntPtr j,IntPtr p);
    [DllImport("kernel32.dll")] static extern bool TerminateJobObject(IntPtr j,uint c);
    [DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr p,uint c);
    [DllImport("kernel32.dll")] static extern uint ResumeThread(IntPtr t);
    [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr h,uint t);
    [DllImport("kernel32.dll")] static extern bool GetExitCodeProcess(IntPtr p,out uint c);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll")] static extern bool CreatePipe(out IntPtr r,out IntPtr w,ref SA a,uint n);
    [DllImport("kernel32.dll")] static extern bool SetHandleInformation(IntPtr h,uint m,uint f);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] static extern IntPtr CreateFile(string n,uint a,uint s,ref SA sa,uint d,uint f,IntPtr t);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool CreateProcess(
        string app,StringBuilder cmd,IntPtr pa,IntPtr ta,bool inherit,uint flags,IntPtr env,string cwd,ref SIX si,out PI pi);
    [DllImport("kernel32.dll")] static extern bool ReadFile(SafeFileHandle h,byte[] b,uint n,out uint read,IntPtr o);
    [DllImport("kernel32.dll")] static extern bool InitializeProcThreadAttributeList(IntPtr list,int count,int flags,ref IntPtr size);
    [DllImport("kernel32.dll")] static extern bool UpdateProcThreadAttribute(IntPtr list,uint flags,IntPtr key,IntPtr value,IntPtr size,IntPtr previous,IntPtr returned);
    [DllImport("kernel32.dll")] static extern void DeleteProcThreadAttributeList(IntPtr list);

    static bool Empty(IntPtr job) {
        Accounting a;
        return QueryInformationJobObject(job,1,out a,(uint)Marshal.SizeOf<Accounting>(),IntPtr.Zero) && a.Active == 0;
    }
    static bool WaitEmpty(IntPtr job,int ms) {
        var clock=Stopwatch.StartNew();
        do { if (Empty(job)) return true; Thread.Sleep(20); } while(clock.ElapsedMilliseconds < ms);
        return Empty(job);
    }
    static string Quote(string value) {
        var b=new StringBuilder("\""); int slashes=0;
        foreach(char c in value) {
            if(c=='\\') { slashes++; continue; }
            if(c=='"') { b.Append('\\',slashes*2+1); b.Append(c); }
            else { b.Append('\\',slashes); b.Append(c); }
            slashes=0;
        }
        b.Append('\\',slashes*2); return b.Append('"').ToString();
    }
    public static Result Run(string executable,string[] args,int seconds,bool capture) {
        return RunCore(executable,args,seconds,capture,null);
    }
    // The existing unnamed-job entry point retains its behavior. This separate
    // entry point permits a fixed child adapter to prove actual PID membership.
    // The caller generates 256 random bits per invocation and never publishes
    // the name. Other users receive no access; the current SID receives only
    // JOB_OBJECT_QUERY through an explicitly protected DACL, not termination,
    // assignment or attribute-changing rights. The creator keeps its handle.
    public static Result RunNamed(string executable,string[] args,int seconds,bool capture,string jobName) {
        if(jobName==null || !Regex.IsMatch(jobName,@"\ALocal\\BambuNativeScale-[0-9a-f]{64}\z"))
            return new Result { Terminated=true, ProcessStage=Stage.InvalidName };
        return RunCore(executable,args,seconds,capture,jobName);
    }
    static Result RunCore(string executable,string[] args,int seconds,bool capture,string jobName) {
        var result=new Result();
        IntPtr job=IntPtr.Zero, read=IntPtr.Zero, write=IntPtr.Zero, nul=IntPtr.Zero;
        IntPtr attributes=IntPtr.Zero, inherited=IntPtr.Zero;
        bool attributesReady=false;
        SafeFileHandle readerHandle=null;
        PI pi=new PI(); bool created=false, assigned=false;
        Task reader=null; int overflow=0; byte[] bytes=null;
        try {
            result.ProcessStage=Stage.Job;
            if(jobName==null) job=CreateJobObject(IntPtr.Zero,null);
            else {
                IntPtr descriptor=IntPtr.Zero;
                try {
                    string sid;
                    using(var identity=WindowsIdentity.GetCurrent()) sid=identity.User.Value;
                    uint size;
                    if(!ConvertStringSecurityDescriptorToSecurityDescriptor(
                        // OWNER RIGHTS suppresses the owner's otherwise implicit
                        // READ_CONTROL/WRITE_DAC grant on subsequently opened handles.
                        "D:P(A;;0x0004;;;OW)(A;;0x0004;;;"+sid+")",1,out descriptor,out size)) throw new Exception();
                    var security=new SA { Length=Marshal.SizeOf<SA>(), Descriptor=descriptor, Inherit=0 };
                    job=CreateJobObject(ref security,jobName);
                    // Never adopt a pre-existing object, even when its name and
                    // access rights happen to fit the current request.
                    if(job==IntPtr.Zero || Marshal.GetLastWin32Error()==183) throw new Exception();
                } finally { if(descriptor!=IntPtr.Zero) LocalFree(descriptor); }
            }
            if(job==IntPtr.Zero) throw new Exception();
            result.ProcessStage=Stage.Limits;
            var limit=new Extended(); limit.Basic.Flags=0x2000; // KILL_ON_JOB_CLOSE, no breakaway.
            if(!SetInformationJobObject(job,9,ref limit,(uint)Marshal.SizeOf<Extended>())) throw new Exception();
            result.ProcessStage=Stage.Streams;
            var sa=new SA { Length=Marshal.SizeOf<SA>(), Inherit=1 };
            nul=CreateFile("NUL",0xC0000000,3,ref sa,3,0,IntPtr.Zero);
            if(nul==new IntPtr(-1)) throw new Exception();
            if(capture && (!CreatePipe(out read,out write,ref sa,4096) || !SetHandleInformation(read,1,0))) throw new Exception();
            result.ProcessStage=Stage.Attributes;
            IntPtr attributeSize=IntPtr.Zero;
            InitializeProcThreadAttributeList(IntPtr.Zero,1,0,ref attributeSize);
            if(attributeSize==IntPtr.Zero) throw new Exception();
            attributes=Marshal.AllocHGlobal(attributeSize);
            if(!InitializeProcThreadAttributeList(attributes,1,0,ref attributeSize)) throw new Exception();
            attributesReady=true;
            // Explicitly inherit only the intended standard streams. NUL is
            // shared by stdin/stderr and by stdout when output is discarded.
            int handleCount=capture ? 2 : 1;
            inherited=Marshal.AllocHGlobal(handleCount*IntPtr.Size);
            Marshal.WriteIntPtr(inherited,nul);
            if(capture) Marshal.WriteIntPtr(inherited,IntPtr.Size,write);
            if(!UpdateProcThreadAttribute(attributes,0,new IntPtr(0x20002),inherited,
                new IntPtr(handleCount*IntPtr.Size),IntPtr.Zero,IntPtr.Zero)) throw new Exception();
            var si=new SIX { Start=new SI { Size=Marshal.SizeOf<SIX>(), Flags=0x101, Show=0, Input=nul,
                Output=capture ? write : nul, Error=nul }, Attributes=attributes };
            result.ProcessStage=Stage.Command;
            var command=new StringBuilder(Quote(executable));
            foreach(var arg in args) command.Append(' ').Append(Quote(arg));
            // The first instruction cannot run before containment is installed.
            result.ProcessStage=Stage.Create;
            if(!CreateProcess(executable,command,IntPtr.Zero,IntPtr.Zero,true,0x08080004,
                IntPtr.Zero,null,ref si,out pi)) {
                result.NativeError=Marshal.GetLastWin32Error(); throw new Exception();
            }
            created=true;
            result.ProcessStage=Stage.Assign;
            if(!AssignProcessToJobObject(job,pi.Process)) throw new Exception();
            assigned=true;
            if(capture) {
                result.ProcessStage=Stage.Reader;
                CloseHandle(write); write=IntPtr.Zero;
                // Transfer ownership before scheduling. Neither process timeout
                // nor drain timeout may close a handle a queued reader will use.
                readerHandle=new SafeFileHandle(read,true);
                read=IntPtr.Zero;
                SafeFileHandle pipe=readerHandle;
                reader=Task.Run(() => {
                    try {
                        using(var stream=new MemoryStream()) {
                            var buffer=new byte[4096]; uint count;
                            while(ReadFile(pipe,buffer,(uint)buffer.Length,out count,IntPtr.Zero) && count != 0) {
                                if(stream.Length+count > 65536) { Interlocked.Exchange(ref overflow,1); return; }
                                stream.Write(buffer,0,(int)count);
                            }
                            bytes=stream.ToArray();
                        }
                    } catch { Interlocked.Exchange(ref overflow,1); }
                    finally { pipe.Dispose(); }
                });
            }
            result.ProcessStage=Stage.Resume;
            if(ResumeThread(pi.Thread)==uint.MaxValue) throw new Exception();
            var deadline=Stopwatch.StartNew();
            result.ProcessStage=Stage.Observe;
            while(!Empty(job) && deadline.ElapsedMilliseconds < seconds*1000L && Volatile.Read(ref overflow)==0) Thread.Sleep(20);
            if(!Empty(job) || Volatile.Read(ref overflow)!=0) {
                result.ProcessStage=Volatile.Read(ref overflow)!=0 ? Stage.OutputLimit : Stage.Timeout;
                TerminateJobObject(job,2); result.Terminated=WaitEmpty(job,5000); return result;
            }
            result.Terminated=true;
            uint code;
            result.ProcessStage=Stage.ExitQuery;
            if(!GetExitCodeProcess(pi.Process,out code) || code==259) return result;
            if(capture) {
                // A descendant holding a pipe cannot extend this deadline. The
                // process-tree verdict is separate from output acceptance.
                result.ProcessStage=Stage.Drain;
                if(!reader.Wait(1000)) { result.ProcessStage=Stage.DrainTimeout; return result; }
                if(Volatile.Read(ref overflow)!=0 || bytes==null) { result.ProcessStage=Stage.DrainRejected; return result; }
                result.ProcessStage=Stage.Decode;
                result.Output=Encoding.UTF8.GetString(bytes);
            }
            result.Code=unchecked((int)code);
            result.ProcessStage=Stage.Complete;
            return result;
        } catch {
            if(assigned) { TerminateJobObject(job,2); result.Terminated=WaitEmpty(job,5000); }
            else if(created) { TerminateProcess(pi.Process,2); result.Terminated=WaitForSingleObject(pi.Process,5000)==0; }
            else result.Terminated=true; // No process was created.
            return result;
        } finally {
            // Closing this job also stops its descendants if a query or explicit
            // termination failed. Such a case remains unverified to the caller.
            if(job!=IntPtr.Zero) CloseHandle(job);
            if(write!=IntPtr.Zero) CloseHandle(write);
            if(read!=IntPtr.Zero) CloseHandle(read);
            // Task.Run throwing is the sole case where ownership was transferred
            // but no reader exists. Otherwise the reader alone disposes its pipe,
            // including after a bounded drain timeout returns unavailable.
            if(reader==null && readerHandle!=null) readerHandle.Dispose();
            if(attributesReady) DeleteProcThreadAttributeList(attributes);
            if(attributes!=IntPtr.Zero) Marshal.FreeHGlobal(attributes);
            if(inherited!=IntPtr.Zero) Marshal.FreeHGlobal(inherited);
            if(nul!=IntPtr.Zero && nul!=new IntPtr(-1)) CloseHandle(nul);
            if(pi.Thread!=IntPtr.Zero) CloseHandle(pi.Thread);
            if(pi.Process!=IntPtr.Zero) CloseHandle(pi.Process);
        }
    }
}
