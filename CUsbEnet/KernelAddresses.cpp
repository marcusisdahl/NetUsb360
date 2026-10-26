#include "stdafx.h"

static const USBENET_KERNEL_ADDRESSES KernelAddressTable[] = {
    {
        17559, FALSE,
        0x801864B8,             // NicEmac
        0x801864B8 + 0x0C,      // TitleEthernetAddress
        0,                      // DebugEthernetAddress
        0,                      // XnInstances
        0x81A86C2C,             // StandardNicDeviceFlags
        0,                      // XbdmNicDeviceFlags
        0x801A7FA0,             // UsbdUnrecognizedPortEntries
        0x800D7D20,             // UsbdReportDeviceConnect
        0x800E06D0,             // UsbdEhciShutdown
        0x800D5B00,             // NicBaseInitialize
        0x800D5B60,             // NicBaseShutdown
        0x800D5AD8,             // NicBaseTimerStart
        0x800D1FB8,             // NicBaseAddInternalStats
        0x800D5B98,             // NicBaseIsSubscribedMcastAddr
        0,                      // NicBasePrintStats
        0,                      // NicBaseTakeLock
        0                       // NicBaseTakeLockAtRaisedIrql
    },
    {
        17559, TRUE,
        0,                      // NicEmac
        0x801E723C,             // TitleEthernetAddress
        0x801E74B4,             // DebugEthernetAddress
        0x81D6AF78,             // XnInstances
        0x81D27614,             // StandardNicDeviceFlags
        0x81D27618,             // XbdmNicDeviceFlags
        0x8020A8C0,             // UsbdUnrecognizedPortEntries
        0x8010C858,             // UsbdReportDeviceConnect
        0x80116388,             // UsbdEhciShutdown
        0x8010A260,             // NicBaseInitialize
        0x8010A2D0,             // NicBaseShutdown
        0x8010A170,             // NicBaseTimerStart
        0x80104BF8,             // NicBaseAddInternalStats
        0x8010A308,             // NicBaseIsSubscribedMcastAddr
        0x8010A398,             // NicBasePrintStats
        0x80106768,             // NicBaseTakeLock
        0x80106858              // NicBaseTakeLockAtRaisedIrql
    }
};

WORD UsbEnetGetKernelBuild() {
    // XboxKrnlVersion (export 344) is data: the export points at the version.
    PXBOX_KRNL_VERSION Version = reinterpret_cast<PXBOX_KRNL_VERSION>(Utilities::ResolveFunction("xboxkrnl.exe", 344));

    if (Version == NULL || !MmIsAddressValid(Version))
        return 0;

    return Version->Build;
}

const USBENET_KERNEL_ADDRESSES* UsbEnetFindKernelAddresses(WORD KernelBuild, BOOL Devkit) {
    for (DWORD Index = 0; Index < ARRAYSIZE(KernelAddressTable); Index++) {
        if (KernelAddressTable[Index].KernelBuild == KernelBuild && (KernelAddressTable[Index].Devkit != FALSE) == (Devkit != FALSE))
            return &KernelAddressTable[Index];
    }

    return NULL;
}
