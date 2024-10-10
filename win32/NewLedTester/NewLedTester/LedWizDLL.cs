using System;
using System.IO;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
using System.Windows.Forms;

// C# interface to LEDWIZ.DLL.
//
// Note that we don't use the conventional declarative DLL linkage that C# makes
// fairly straightforward via [DllImport()].  We instead explicitly load the
// DLL dynamically (via Win32 LoadLibrary()) and link to the entrypoints
// dynamically (via Win32 GetProcAddr()).  The more straightforward declarative
// linkage unfortunately doesn't work quite right with the LWCloneU2 version of
// LEDWIZ.EXE thanks to its worker thread.  The clone DLL starts a worker thread
// when loaded, and terminates it when the DLL is unloaded.  With normal "unmanaged
// code" clients (e.g., C or C++), that works fine.  With "managed" code (C#), the
// thread setup has a problematic interaction with some versions of the .NET runtime.
// In particular, some versions of the runtime will effectively deadlock with the
// DLL at process termination time: the runtime will wait for all threads to exit
// before exiting the process, but the worker thread in the DLL won't terminate
// until the host process reaches the point in the termination sequence where it
// unloads DLLs.  So the two sit there waiting for the other to go first, and the 
// process never exits.  I've seen different behavior on different machines, from 
// which I infer that it's a function of the .NET runtime version.  It's possible
// to work around this hackily using the declarative loader as normal, by asking
// LoadLibrary for a handle to the DLL at process exit time and then unloading
// the handle *twice*: once for the explicit LoadLibrary call and once for the
// handle that the declarative loader got for its own use.  That works, but only
// on .NET versions with the deadlock problem; on other versions, the runtime
// was going to unload the DLL anyway, so the extra unload is one too many, and
// crashes the process.  The only clean workaround I've found is to do all of the
// DLL module management explicitly.  It's more work, but it does things by the
// book (no questionable hacks involved) and works reliably.
//
namespace NewLedTester
{
    public class LedWizDLL
    {
        public LedWizDLL()
        {
            Load();
        }

        ~LedWizDLL()
        {
            Unload();
        }

        private void Load()
        {
            hModule = LoadLibrary(IntPtr.Size == 4 ? @".\ledwiz.dll" : @".\ledwiz64.dll");
            if (hModule == IntPtr.Zero)
            {

                throw new Exception(
                    "Unable to load LEDWIZ.DLL (Windows error " 
                    + Marshal.GetLastWin32Error() + ")");
            }

            LWZ_SBA = (_LWZ_SBA)procAddr("LWZ_SBA", typeof(_LWZ_SBA));
            LWZ_PBA = (_LWZ_PBA)procAddr("LWZ_PBA", typeof(_LWZ_PBA));
            LWZ_REGISTER = (_LWZ_REGISTER)procAddr("LWZ_REGISTER", typeof(_LWZ_REGISTER));
            LWZ_SET_NOTIFY = (_LWZ_SET_NOTIFY)procAddr("LWZ_SET_NOTIFY", typeof(_LWZ_SET_NOTIFY));
            LWZ_SET_NOTIFY_IntPtr = (_LWZ_SET_NOTIFY_IntPtr)procAddr("LWZ_SET_NOTIFY", typeof(_LWZ_SET_NOTIFY_IntPtr));
            LWZ_GET_DEVICE_INFO = (_LWZ_GET_DEVICE_INFO)procAddr("LWZ_GET_DEVICE_INFO", typeof(_LWZ_GET_DEVICE_INFO));
        }

        private void Unload()
        {
            if (hModule != IntPtr.Zero)
            {
                FreeLibrary(hModule);
                hModule = IntPtr.Zero;
            }

            LWZ_SBA = null;
            LWZ_PBA = null;
            LWZ_REGISTER = null;
            LWZ_SET_NOTIFY = null;
            LWZ_SET_NOTIFY_IntPtr = null;
            LWZ_GET_DEVICE_INFO = null;
        }

        public void Reload()
        {
            if (hModule != IntPtr.Zero)
                FreeLibrary(hModule);

            Load();
        }

        Delegate procAddr(string name, Type t)
        {
            IntPtr addr = GetProcAddress(hModule, name);
            if (addr == IntPtr.Zero)
                return null;
            else
                return Marshal.GetDelegateForFunctionPointer(addr, t);
        }

        // dll handle
        IntPtr hModule = IntPtr.Zero;

        // pointers to the DLL entrypoints, via delegates
        public _LWZ_SBA LWZ_SBA;
        public _LWZ_PBA LWZ_PBA;
        public _LWZ_REGISTER LWZ_REGISTER;
        public _LWZ_SET_NOTIFY LWZ_SET_NOTIFY;
        public _LWZ_SET_NOTIFY_IntPtr LWZ_SET_NOTIFY_IntPtr;
        public _LWZ_GET_DEVICE_INFO LWZ_GET_DEVICE_INFO;

        // maximum number of devices
        const int LWZ_MAX_DEVICES = 16;

        [StructLayout(LayoutKind.Sequential)]
        public struct LWZDEVICELIST
        {
            unsafe fixed UInt32 handles[LWZ_MAX_DEVICES];
            UInt32 numdevices;
        }

        // device type codes for LWZDEVICEINFO
        public const UInt32 LWZ_DEVICE_TYPE_NONE = 0;
        public const UInt32 LWZ_DEVICE_TYPE_LEDWIZ = 1;
        public const UInt32 LWZ_DEVICE_TYPE_LWCLONEU2 = 2;
        public const UInt32 LWZ_DEVICE_TYPE_PINSCAPE = 3;
        public const UInt32 LWZ_DEVCIE_TYPE_PINSCAPE_VIRT = 4;

        [StructLayout(LayoutKind.Sequential)]
        public struct LWZDEVICEINFO
        {
            public UInt32 cbSize;
            public UInt32 dwDevType;
            public unsafe fixed sbyte szName[256];
        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void _LWZ_SBA(
            UInt32 hlwz, 
            UInt32 bank0, 
            UInt32 bank1, 
            UInt32 bank2, 
            UInt32 bank3, 
            UInt32 speed);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void _LWZ_PBA(
            UInt32 hlwz,
            byte[] mode32bytes);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void _LWZ_REGISTER(
            UInt32 hlwz,
            IntPtr hwnd);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void _LWZ_SET_NOTIFY(
            LWZNOTIFYPROC notify_callback,
            ref LWZDEVICELIST plist);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void _LWZ_SET_NOTIFY_IntPtr(
            IntPtr notify_callback,
            IntPtr plist);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void _LWZ_GET_DEVICE_INFO(
            UInt32 hlwz,
            ref LWZDEVICEINFO info);

        public void LWZ_UNSET_NOTIFY()
        {
            LWZ_SET_NOTIFY_IntPtr(IntPtr.Zero, IntPtr.Zero);
        }

        public const int REASON_ADD = 1;
        public const int REASON_DELETE = 2;
        public delegate void LWZNOTIFYPROC(Int32 reason, UInt32 handle);

        [DllImport("kernel32.dll")]
        public static extern IntPtr LoadLibrary(string dllToLoad);

        [DllImport("kernel32.dll")]
        public static extern bool FreeLibrary(IntPtr hModule);

        [DllImport("kernel32.dll")]
        public static extern IntPtr GetProcAddress(IntPtr hModule, string procedureName);
    }
}
