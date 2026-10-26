#pragma once

// Every fixed kernel address NetUsb360 uses, for one kernel build and kind.
// Main::Init picks the entry that matches the running kernel and refuses to
// start when none does. To support another kernel, add an entry to the table
// in KernelAddresses.cpp; leave a field 0 when that kernel kind has no such
// object (the retail kernel has no debug XNet instance, for example).
typedef struct _USBENET_KERNEL_ADDRESSES {
    WORD KernelBuild;
    BOOL Devkit;
    DWORD NicEmac;                      // g_NicEmac user-list head (retail)
    DWORD TitleEthernetAddress;
    DWORD DebugEthernetAddress;         // devkit only
    DWORD XnInstances;                  // devkit only
    DWORD StandardNicDeviceFlags;
    DWORD XbdmNicDeviceFlags;           // devkit only
    DWORD UsbdUnrecognizedPortEntries;
    DWORD UsbdReportDeviceConnect;
    DWORD UsbdEhciShutdown;
    DWORD NicBaseInitialize;
    DWORD NicBaseShutdown;
    DWORD NicBaseTimerStart;
    DWORD NicBaseAddInternalStats;
    DWORD NicBaseIsSubscribedMcastAddr;
    DWORD NicBasePrintStats;            // devkit only
    DWORD NicBaseTakeLock;              // devkit only
    DWORD NicBaseTakeLockAtRaisedIrql;  // devkit only
} USBENET_KERNEL_ADDRESSES, *PUSBENET_KERNEL_ADDRESSES;

// Returns the running kernel's build number, or 0 if it can't be read.
WORD UsbEnetGetKernelBuild();
const USBENET_KERNEL_ADDRESSES* UsbEnetFindKernelAddresses(WORD KernelBuild, BOOL Devkit);
