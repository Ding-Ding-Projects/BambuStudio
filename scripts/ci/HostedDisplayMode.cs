// Used only on disposable hosted Windows machines by the fixed scale probe.
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;

public static class HostedDisplayMode
{
    const uint Allowed=0x207c00a0, Required=0x007c0000;
    public sealed class State { public string Device, Identity; public byte[] Original; }
    public sealed class Outcome { public int? TestCode, ApplyCode; public bool Verified, AlreadyCurrent; }
    [StructLayout(LayoutKind.Sequential)] struct RECT { public int L,T,R,B; }
    [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct MONITOR {
        public uint Size; public RECT Bounds,Work; public uint Flags;
        [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)] public string Device;
    }
    [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct DEVICE {
        public uint Size;
        [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)] public string Name;
        [MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)] public string Description;
        public uint Flags;
        [MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)] public string Id;
        [MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)] public string Key;
    }
    [StructLayout(LayoutKind.Explicit,Size=72)] struct PATH {
        [FieldOffset(0)] public uint SourceLow; [FieldOffset(4)] public int SourceHigh;
        [FieldOffset(8)] public uint SourceId; [FieldOffset(20)] public uint TargetLow;
        [FieldOffset(24)] public int TargetHigh; [FieldOffset(28)] public uint TargetId;
        [FieldOffset(68)] public uint Flags;
    }
    [StructLayout(LayoutKind.Explicit,Size=64)] struct MODEINFO { [FieldOffset(0)] public uint Type; }
    [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct SOURCE {
        public uint Type,Size,Low; public int High; public uint Id;
        [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)] public string Name;
    }
    [DllImport("user32.dll")] static extern IntPtr MonitorFromWindow(IntPtr window,uint flags);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern bool GetMonitorInfo(IntPtr monitor,ref MONITOR info);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern bool EnumDisplayDevices(string device,uint index,ref DEVICE value,uint flags);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern bool EnumDisplaySettings(string device,int index,IntPtr mode);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int ChangeDisplaySettingsEx(string device,IntPtr mode,IntPtr window,uint flags,IntPtr parameter);
    [DllImport("user32.dll")] static extern int GetDisplayConfigBufferSizes(uint flags,out uint paths,out uint modes);
    [DllImport("user32.dll")] static extern int QueryDisplayConfig(uint flags,ref uint paths,[Out] PATH[] pathArray,ref uint modes,[Out] MODEINFO[] modeArray,IntPtr topology);
    [DllImport("user32.dll",ExactSpelling=true)] static extern int DisplayConfigGetDeviceInfo(ref SOURCE source);
    static void Require(bool ok) { if(!ok) throw new InvalidOperationException("Display mode contract unavailable."); }
    static uint U(byte[] bytes,int offset) { return BitConverter.ToUInt32(bytes,offset); }
    static void Put(byte[] bytes,int offset,uint value) { Array.Copy(BitConverter.GetBytes(value),0,bytes,offset,4); }
    public static int Width(byte[] bytes) { Validate(bytes); return checked((int)U(bytes,172)); }
    public static int Height(byte[] bytes) { Validate(bytes); return checked((int)U(bytes,176)); }
    static void Validate(byte[] bytes) {
        Require(bytes!=null && bytes.Length==220 && BitConverter.ToUInt16(bytes,68)==220 && BitConverter.ToUInt16(bytes,70)==0);
        uint fields=U(bytes,72);
        Require((fields & ~Allowed)==0 && (fields & Required)==Required);
        Require(U(bytes,168)==32 && U(bytes,172)>0 && U(bytes,172)<=8192 && U(bytes,176)>0 && U(bytes,176)<=8192);
        Require(U(bytes,184)>0 && U(bytes,184)<=1000);
        if((fields & 0x80)!=0) Require(U(bytes,84)<=3);
    }
    static byte[] Read(string device,int index) {
        IntPtr memory=Marshal.AllocHGlobal(220);
        try {
            Marshal.Copy(new byte[220],0,memory,220); Marshal.WriteInt16(memory,68,220);
            if(!EnumDisplaySettings(device,index,memory)) return null;
            var bytes=new byte[220]; Marshal.Copy(memory,bytes,0,220); Validate(bytes); return bytes;
        } finally { Marshal.FreeHGlobal(memory); }
    }
    static State Active() {
        for(int attempt=0;attempt<2;attempt++) {
            uint paths,modes;
            Require(GetDisplayConfigBufferSizes(2,out paths,out modes)==0 && paths>0 && paths<=64 && modes>0 && modes<=256);
            var p=new PATH[paths]; var m=new MODEINFO[modes];
            int result=QueryDisplayConfig(2,ref paths,p,ref modes,m,IntPtr.Zero);
            if(result==122) continue;
            Require(result==0 && paths==1 && (p[0].Flags & 1)!=0);
            var source=new SOURCE { Type=1,Size=(uint)Marshal.SizeOf<SOURCE>(),Low=p[0].SourceLow,High=p[0].SourceHigh,Id=p[0].SourceId };
            Require(DisplayConfigGetDeviceInfo(ref source)==0 && !String.IsNullOrEmpty(source.Name));
            bool found=false;
            for(uint i=0;i<64;i++) {
                var d=new DEVICE { Size=(uint)Marshal.SizeOf<DEVICE>() };
                if(!EnumDisplayDevices(null,i,ref d,0)) break;
                if(String.Equals(d.Name,source.Name,StringComparison.OrdinalIgnoreCase)) {
                    Require((d.Flags & 5)==5 && (d.Flags & 8)==0); found=true; break;
                }
            }
            Require(found);
            return new State { Device=source.Name, Identity=p[0].SourceLow+":"+p[0].SourceHigh+":"+p[0].SourceId+":"+
                p[0].TargetLow+":"+p[0].TargetHigh+":"+p[0].TargetId };
        }
        throw new InvalidOperationException("Display topology did not stabilize.");
    }
    public static State Capture(IntPtr window) {
        var state=Active();
        IntPtr monitor=MonitorFromWindow(window,0);
        var info=new MONITOR { Size=(uint)Marshal.SizeOf<MONITOR>() };
        Require(monitor!=IntPtr.Zero && GetMonitorInfo(monitor,ref info) && (info.Flags & 1)!=0 &&
            String.Equals(info.Device,state.Device,StringComparison.OrdinalIgnoreCase));
        state.Original=Read(state.Device,-1); Validate(state.Original); AssertBinding(state); return state;
    }
    public static State Recover(string device,string identity,byte[] original) {
        Require(device!=null && device.Length<=32 && identity!=null && identity.Length<=160);
        Validate(original);
        var state=new State { Device=device,Identity=identity,Original=original };
        AssertBinding(state); return state;
    }
    public static void AssertBinding(State state) {
        Require(state!=null); var active=Active();
        Require(String.Equals(active.Device,state.Device,StringComparison.OrdinalIgnoreCase) && active.Identity==state.Identity);
    }
    static readonly uint[] Bits={0x20,0x80,0x40000,0x80000,0x100000,0x200000,0x400000,0x20000000};
    static readonly int[] Offsets={76,84,168,172,176,180,184,88};
    static bool Matches(byte[] observed,byte[] wanted) {
        Validate(observed); Validate(wanted);
        uint fields=U(wanted,72);
        if((U(observed,72) & fields)!=fields) return false;
        for(int i=0;i<Bits.Length;i++) if((fields & Bits[i])!=0) {
            if(U(observed,Offsets[i])!=U(wanted,Offsets[i])) return false;
            if(Bits[i]==0x20 && U(observed,80)!=U(wanted,80)) return false;
        }
        return true;
    }
    public static byte[] Target(State state) {
        AssertBinding(state); Validate(state.Original);
        Require((U(state.Original,72) & 0x80)!=0); // Preserve an observed orientation, never invent one.
        byte[] selected=null; bool complete=false;
        for(int i=0;i<512;i++) {
            var candidate=Read(state.Device,i);
            if(candidate==null) { complete=true; break; }
            if(U(candidate,172)!=1920 || U(candidate,176)!=1080 || U(candidate,168)!=32 ||
                U(candidate,184)!=U(state.Original,184) || (U(candidate,72) & 0x80)==0 ||
                U(candidate,84)!=U(state.Original,84) || U(candidate,180)!=U(state.Original,180)) continue;
            // Preserve current placement and fixed-output semantics when the
            // enumerated mode leaves those fields unspecified.
            foreach(int index in new int[]{0,7}) if((U(state.Original,72) & Bits[index])!=0) {
                if((U(candidate,72) & Bits[index])!=0) {
                    Require(U(candidate,Offsets[index])==U(state.Original,Offsets[index]));
                    if(index==0) Require(U(candidate,80)==U(state.Original,80));
                } else {
                    Put(candidate,Offsets[index],U(state.Original,Offsets[index]));
                    if(index==0) Put(candidate,80,U(state.Original,80));
                    Put(candidate,72,U(candidate,72)|Bits[index]);
                }
            }
            Require(selected==null || (Matches(candidate,selected) && Matches(selected,candidate)));
            selected=candidate;
        }
        Require(complete && selected!=null); return selected;
    }
    static int Change(State state,byte[] desired,uint flags) {
        AssertBinding(state); Validate(desired);
        IntPtr memory=Marshal.AllocHGlobal(220);
        try { Marshal.Copy(desired,0,memory,220); return ChangeDisplaySettingsEx(state.Device,memory,IntPtr.Zero,flags,IntPtr.Zero); }
        finally { Marshal.FreeHGlobal(memory); }
    }
    public static Outcome Apply(State state,byte[] desired) {
        AssertBinding(state); Validate(desired);
        if(Matches(Read(state.Device,-1),desired)) return new Outcome { Verified=true,AlreadyCurrent=true };
        var outcome=new Outcome { TestCode=Change(state,desired,2) }; // CDS_TEST, never persists.
        if(outcome.TestCode==0) outcome.ApplyCode=Change(state,desired,0); // Dynamic change only.
        // Always inspect actual state after the attempt, including a nonzero
        // result. Original recovery state remains durable outside this method.
        bool matches=false;
        for(int i=0;i<40;i++) {
            AssertBinding(state); matches=Matches(Read(state.Device,-1),desired);
            if(matches || outcome.TestCode!=0 || outcome.ApplyCode!=0) break;
            Thread.Sleep(250);
        }
        outcome.Verified=outcome.TestCode==0 && outcome.ApplyCode==0 && matches;
        return outcome;
    }
    public static bool OriginalCurrent(State state) { AssertBinding(state); return Matches(Read(state.Device,-1),state.Original); }
}
