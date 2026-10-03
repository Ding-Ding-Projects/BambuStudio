// Used only by Invoke-HostedDisplayScale.ps1 on disposable hosted Windows jobs.
using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

public static class HostedScaleProcess
{
    public sealed class Result { public bool Terminated; public int Code = -1; public string Output = ""; }
    [StructLayout(LayoutKind.Sequential)] struct SA { public int Length; public IntPtr Descriptor; public int Inherit; }
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct SI {
        public int Size; public string Reserved, Desktop, Title;
        public int X,Y,XSize,YSize,XChars,YChars,Fill,Flags;
        public short Show, ReservedSize; public IntPtr Reserved2, Input, Output, Error;
    }
    [StructLayout(LayoutKind.Sequential)] struct PI { public IntPtr Process, Thread; public uint Pid, Tid; }
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
        string app,StringBuilder cmd,IntPtr pa,IntPtr ta,bool inherit,uint flags,IntPtr env,string cwd,ref SI si,out PI pi);
    [DllImport("kernel32.dll")] static extern bool ReadFile(IntPtr h,byte[] b,uint n,out uint read,IntPtr o);

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
        var result=new Result();
        IntPtr job=IntPtr.Zero, read=IntPtr.Zero, write=IntPtr.Zero, nul=IntPtr.Zero;
        PI pi=new PI(); bool created=false, assigned=false;
        Task reader=null; int overflow=0; byte[] bytes=null;
        try {
            job=CreateJobObject(IntPtr.Zero,null);
            if(job==IntPtr.Zero) throw new Exception();
            var limit=new Extended(); limit.Basic.Flags=0x2000; // KILL_ON_JOB_CLOSE, no breakaway.
            if(!SetInformationJobObject(job,9,ref limit,(uint)Marshal.SizeOf<Extended>())) throw new Exception();
            var sa=new SA { Length=Marshal.SizeOf<SA>(), Inherit=1 };
            nul=CreateFile("NUL",0xC0000000,3,ref sa,3,0,IntPtr.Zero);
            if(nul==new IntPtr(-1)) throw new Exception();
            if(capture && (!CreatePipe(out read,out write,ref sa,4096) || !SetHandleInformation(read,1,0))) throw new Exception();
            var si=new SI { Size=Marshal.SizeOf<SI>(), Flags=0x101, Show=0, Input=nul,
                Output=capture ? write : nul, Error=nul };
            var command=new StringBuilder(Quote(executable));
            foreach(var arg in args) command.Append(' ').Append(Quote(arg));
            // The first instruction cannot run before containment is installed.
            if(!CreateProcess(executable,command,IntPtr.Zero,IntPtr.Zero,true,0x08000004,
                IntPtr.Zero,null,ref si,out pi)) throw new Exception();
            created=true;
            if(!AssignProcessToJobObject(job,pi.Process)) throw new Exception();
            assigned=true;
            if(capture) {
                CloseHandle(write); write=IntPtr.Zero;
                IntPtr pipe=read;
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
                });
            }
            if(ResumeThread(pi.Thread)==uint.MaxValue) throw new Exception();
            var deadline=Stopwatch.StartNew();
            while(!Empty(job) && deadline.ElapsedMilliseconds < seconds*1000L && Volatile.Read(ref overflow)==0) Thread.Sleep(20);
            if(!Empty(job) || Volatile.Read(ref overflow)!=0) {
                TerminateJobObject(job,2); result.Terminated=WaitEmpty(job,5000); return result;
            }
            result.Terminated=true;
            uint code;
            if(!GetExitCodeProcess(pi.Process,out code) || code==259) return result;
            if(capture) {
                // A descendant holding a pipe cannot extend this deadline. The
                // process-tree verdict is separate from output acceptance.
                if(!reader.Wait(1000) || Volatile.Read(ref overflow)!=0 || bytes==null) return result;
                result.Output=Encoding.UTF8.GetString(bytes);
            }
            result.Code=unchecked((int)code);
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
            if(nul!=IntPtr.Zero && nul!=new IntPtr(-1)) CloseHandle(nul);
            if(pi.Thread!=IntPtr.Zero) CloseHandle(pi.Thread);
            if(pi.Process!=IntPtr.Zero) CloseHandle(pi.Process);
        }
    }
}
