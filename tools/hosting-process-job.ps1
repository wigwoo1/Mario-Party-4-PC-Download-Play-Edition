# A Windows kill-on-close job owns Node and every hosting/game descendant.
# Create suspended, assign first, then resume: no unowned-child startup window.
if (!("Partyboard.HostingJob" -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
namespace Partyboard {
 public static class HostingJob {
  [StructLayout(LayoutKind.Sequential)] struct Basic {
   public long PerProcess, PerJob; public uint Flags;
   public UIntPtr MinWorking, MaxWorking; public uint Active;
   public UIntPtr Affinity; public uint Priority, Scheduling;
  }
  [StructLayout(LayoutKind.Sequential)] struct Io { public ulong ReadOps, WriteOps, OtherOps, ReadBytes, WriteBytes, OtherBytes; }
  [StructLayout(LayoutKind.Sequential)] struct Extended {
   public Basic Basic; public Io Io; public UIntPtr ProcessMemory, JobMemory, PeakProcess, PeakJob;
  }
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct Startup {
   public uint Size; public string Reserved, Desktop, Title; public uint X,Y,Width,Height,CharsX,CharsY,Fill,Flags;
   public ushort Show,ReservedSize; public IntPtr ReservedData,Input,Output,Error;
  }
  [StructLayout(LayoutKind.Sequential)] struct ProcessInfo { public IntPtr Process,Thread; public uint Pid,Tid; }
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr CreateJobObject(IntPtr security,string name);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetInformationJobObject(IntPtr job,int type,ref Extended info,uint size);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool AssignProcessToJobObject(IntPtr job,IntPtr process);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcess(string application,StringBuilder command,IntPtr ps,IntPtr ts,bool inherit,uint flags,IntPtr environment,string cwd,ref Startup startup,out ProcessInfo process);
  [DllImport("kernel32.dll",SetLastError=true)] static extern uint ResumeThread(IntPtr thread);
  [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr handle,uint timeout);
  [DllImport("kernel32.dll")] static extern bool GetExitCodeProcess(IntPtr process,out uint code);
  [DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr process,uint code);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
  static string Quote(string value) {
   var b=new StringBuilder("\""); int slashes=0;
   foreach(char ch in value) {
    if(ch=='\\') { slashes++; continue; }
    if(ch=='"') b.Append('\\',slashes*2+1); else b.Append('\\',slashes);
    b.Append(ch); slashes=0;
   }
   return b.Append('\\',slashes*2).Append('"').ToString();
  }
  public static int Run(string executable,string[] arguments,string cwd) {
   IntPtr job=CreateJobObject(IntPtr.Zero,null); if(job==IntPtr.Zero) throw new Win32Exception();
   ProcessInfo child=new ProcessInfo();
   try {
    var limits=new Extended(); limits.Basic.Flags=0x2000;
    if(!SetInformationJobObject(job,9,ref limits,(uint)Marshal.SizeOf(typeof(Extended)))) throw new Win32Exception();
    var command=new StringBuilder(Quote(executable)); foreach(string argument in arguments) command.Append(' ').Append(Quote(argument));
    var startup=new Startup(); startup.Size=(uint)Marshal.SizeOf(typeof(Startup));
    if(!CreateProcess(executable,command,IntPtr.Zero,IntPtr.Zero,true,4,IntPtr.Zero,cwd,ref startup,out child)) throw new Win32Exception();
    if(!AssignProcessToJobObject(job,child.Process)) { TerminateProcess(child.Process,1); throw new Win32Exception(); }
    if(ResumeThread(child.Thread)==UInt32.MaxValue) throw new Win32Exception();
    if(WaitForSingleObject(child.Process,UInt32.MaxValue)!=0) throw new Win32Exception();
    uint code; if(!GetExitCodeProcess(child.Process,out code)) throw new Win32Exception(); return unchecked((int)code);
   } finally {
    CloseHandle(job);
    if(child.Thread!=IntPtr.Zero) CloseHandle(child.Thread);
    if(child.Process!=IntPtr.Zero) CloseHandle(child.Process);
   }
  }
 }
}
'@
}
