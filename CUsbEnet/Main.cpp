#include "stdafx.h"

namespace Main {
    HANDLE mHandle = NULL;
    BOOL Devkit = FALSE;
    BOOL Exiting = FALSE;
    volatile LONG InitState = 0; // 0=not started, 1=running, 2=complete
    WORD KernelBuild = 0;
    const USBENET_KERNEL_ADDRESSES* Kernel = NULL;
    static volatile LONG DriverRegistered = 0;

    CEnetAddr* GetKernelTitleEthernetAddress() {
        return reinterpret_cast<CEnetAddr*>(Kernel->TitleEthernetAddress);
    }

    CEnetAddr* GetKernelDebugEthernetAddress() {
        // The free/retail kernel has no debug XNet instance or debug MAC slot.
        return Kernel->DebugEthernetAddress != 0 ? reinterpret_cast<CEnetAddr*>(Kernel->DebugEthernetAddress) : NULL;
    }

    typedef VOID(__fastcall* PUSBD_REPORT_DEVICE_CONNECT)(PUSBD_DEVICE_NODE HubDeviceNode, DWORD Port);
    typedef VOID(__fastcall* PUSBD_TITLE_DRIVER_SET_UNRECOGNIZED_PORT)(PUSBD_DEVICE_NODE DeviceNode, BOOL Set);

    struct USB_REENUMERATION_CONTEXT {
        DWORD ConnectCount;
    };

    struct USB_SET_UNRECOGNIZED_CONTEXT {
        PUSBD_DEVICE_NODE DeviceNode;
        PUSBD_TITLE_DRIVER_SET_UNRECOGNIZED_PORT SetUnrecognizedPort;
        BOOL Completed;
    };

    static VOID SetUsbDeviceUnrecognizedDpc(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2) {
        UNREFERENCED_PARAMETER(Dpc);
        UNREFERENCED_PARAMETER(SystemArgument1);
        UNREFERENCED_PARAMETER(SystemArgument2);

        USB_SET_UNRECOGNIZED_CONTEXT* Context = static_cast<USB_SET_UNRECOGNIZED_CONTEXT*>(DeferredContext);

        if (Context == NULL || Context->DeviceNode == NULL || Context->SetUnrecognizedPort == NULL)
            return;

        Context->SetUnrecognizedPort(Context->DeviceNode, TRUE);
        Context->Completed = TRUE;
    }

    static BOOL SetUsbDeviceUnrecognized(PUSBD_DEVICE_NODE DeviceNode) {
        if (DeviceNode == NULL)
            return FALSE;

        PUSBD_TITLE_DRIVER_SET_UNRECOGNIZED_PORT SetUnrecognizedPort = reinterpret_cast<PUSBD_TITLE_DRIVER_SET_UNRECOGNIZED_PORT>(Utilities::ResolveFunction("xboxkrnl.exe", 891));

        if (SetUnrecognizedPort == NULL) {
            DbgPrint("[CUsbEnet/Main] Could not resolve UsbdTitleDriverSetUnrecognizedPort.\n");
            return FALSE;
        }

        USB_SET_UNRECOGNIZED_CONTEXT Context = { DeviceNode, SetUnrecognizedPort, FALSE };
        NTSTATUS Status = KeCallAndWaitForDpcRoutine(SetUsbDeviceUnrecognizedDpc, &Context, 2, NULL, NULL);

        if (!NT_SUCCESS(Status) || !Context.Completed) {
            DbgPrint("[CUsbEnet/Main] Could not return USB Ethernet device to the unrecognized-port list: status=0x%08X completed=%u.\n", Status, Context.Completed);
            return FALSE;
        }

        DbgPrint("[CUsbEnet/Main] USB Ethernet device returned to the unrecognized-port list for plugin reload.\n");
        return TRUE;
    }

    static VOID UsbReenumerateUnrecognizedPortsDpc(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2) {
        UNREFERENCED_PARAMETER(Dpc);
        UNREFERENCED_PARAMETER(SystemArgument1);
        UNREFERENCED_PARAMETER(SystemArgument2);

        USB_REENUMERATION_CONTEXT* Context = static_cast<USB_REENUMERATION_CONTEXT*>(DeferredContext);
        PUSBD_UNRECOGNIZED_PORT_ENTRY Entries = reinterpret_cast<PUSBD_UNRECOGNIZED_PORT_ENTRY>(Main::Kernel->UsbdUnrecognizedPortEntries);
        PUSBD_REPORT_DEVICE_CONNECT ReportDeviceConnect = reinterpret_cast<PUSBD_REPORT_DEVICE_CONNECT>(Main::Kernel->UsbdReportDeviceConnect);
        DWORD ConnectCount = 0;

        for (DWORD EntryIndex = 0; EntryIndex < USBD_UNRECOGNIZED_PORT_ENTRY_COUNT; EntryIndex++) {
            PUSBD_DEVICE_NODE HubDeviceNode = Entries[EntryIndex].HubDeviceNode;
            BYTE PortMask = Entries[EntryIndex].PortMask;

            if (HubDeviceNode == NULL || PortMask == 0)
                continue;

            DbgPrint("[CUsbEnet/Main] Unrecognized USB entry=%u hub=%p portMask=0x%02X\n", EntryIndex, HubDeviceNode, PortMask);

            for (DWORD Port = 0; PortMask != 0; Port++, PortMask >>= 1) {
                if ((PortMask & 1) == 0)
                    continue;

                DbgPrint("[CUsbEnet/Main] Re-enumerating hub=%p port=%u\n", HubDeviceNode, Port);
                ReportDeviceConnect(HubDeviceNode, Port);
                ConnectCount++;
            }
        }

        if (Context != NULL)
            Context->ConnectCount = ConnectCount;
    }

    NTSTATUS ReenumerateUnrecognizedUsbDevices(DWORD* ConnectCount, BOOL Verbose) {
        USB_REENUMERATION_CONTEXT Context = { 0 };

        if (Verbose)
            DbgPrint("[CUsbEnet/Main] Re-enumerating unrecognized USB devices.\n");

        NTSTATUS Status = KeCallAndWaitForDpcRoutine(UsbReenumerateUnrecognizedPortsDpc, &Context, 2, NULL, NULL);

        if (!NT_SUCCESS(Status)) {
            DbgPrint("[CUsbEnet/Main] KeCallAndWaitForDpcRoutine failed: 0x%08X\n", Status);
            return Status;
        }

        if (ConnectCount != NULL)
            *ConnectCount = Context.ConnectCount;

        if (Verbose || Context.ConnectCount != 0)
            DbgPrint("[CUsbEnet/Main] Queued %u unrecognized USB port(s) for re-enumeration.\n", Context.ConnectCount);

        return STATUS_SUCCESS;
    }

    static DWORD WINAPI UsbReenumerationRetryThread(LPVOID) {
        const DWORD RetryCount = 20;
        const DWORD RetryDelayMilliseconds = 500;

        Sleep(RetryDelayMilliseconds);

        for (DWORD Attempt = 1; Attempt <= RetryCount && !Exiting && g_UsbEnet.DeviceAttached == 0; Attempt++) {
            DWORD ConnectCount = 0;
            BOOL Verbose = Attempt == 1 || Attempt == 5 || Attempt == 10 || Attempt == RetryCount;
            NTSTATUS Status = ReenumerateUnrecognizedUsbDevices(&ConnectCount, Verbose);

            if (!NT_SUCCESS(Status))
                break;

            if (g_UsbEnet.DeviceAttached != 0)
                break;

            if (ConnectCount != 0)
                DbgPrint("[CUsbEnet/Main] USB replay attempt %u queued %u port(s); waiting for AddDevice.\n", Attempt, ConnectCount);

            Sleep(RetryDelayMilliseconds);
        }

        if (!Exiting) {
            if (g_UsbEnet.DeviceAttached != 0)
                DbgPrint("[CUsbEnet/Main] USB Ethernet device attached after deferred re-enumeration.\n");
            else
                DbgPrint("[CUsbEnet/Main] USB Ethernet device was not found after deferred re-enumeration retries.\n");
        }

        return 0;
    }

    VOID Init() {
        USBENET_INFO_PRINT("[NetUsb360] NetUsb360 %d.%d.%d (%s, %s build%s%s) starting.\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, USBENET_BUILD_VARIANT, USBENET_BUILD_KIND, USBENET_PERF_LOGGING ? ", perf logging" : "", USBENET_DEBUG ? ", debug output" : "");

        Devkit = (*(PDWORD)0x8E038610 & 0x8000) ? FALSE : TRUE;
        KernelBuild = UsbEnetGetKernelBuild();
        USBENET_INFO_PRINT("[NetUsb360] Kernel %u, %s.\n", KernelBuild, Devkit ? "devkit" : "retail");

        if ((!Devkit && DEVKIT_ONLY) || (Devkit && !DEVKIT_ONLY)) {
            USBENET_INFO_PRINT("[NetUsb360] This file is built for %s consoles only. Unloading.\n", DEVKIT_ONLY ? "devkit" : "retail");
            XexUnloadImage(mHandle);
            return;
        }

        /* Every hook and structure offset is fixed for one kernel build. On any
         * other kernel they would point at unrelated code, so unload cleanly
         * instead of risking a crash. */
        Kernel = UsbEnetFindKernelAddresses(KernelBuild, Devkit);
        if (Kernel == NULL) {
            USBENET_INFO_PRINT("[NetUsb360] Kernel %u is not supported (only 17559). Unloading.\n", KernelBuild);
            XexUnloadImage(mHandle);
            return;
        }

        Settings::Load(mHandle);

        CEnetAddr* TitleAddress = GetKernelTitleEthernetAddress();
        if (Devkit) {
            CEnetAddr* DebugAddress = GetKernelDebugEthernetAddress();
            DbgPrint("[CUsbEnet/Main] Using checked/dev title MAC address=%p value=%02X:%02X:%02X:%02X:%02X:%02X debug MAC address=%p value=%02X:%02X:%02X:%02X:%02X:%02X.\n", TitleAddress, TitleAddress->_ab[0], TitleAddress->_ab[1], TitleAddress->_ab[2], TitleAddress->_ab[3], TitleAddress->_ab[4], TitleAddress->_ab[5], DebugAddress, DebugAddress->_ab[0], DebugAddress->_ab[1], DebugAddress->_ab[2], DebugAddress->_ab[3], DebugAddress->_ab[4], DebugAddress->_ab[5]);
        } else {
            DbgPrint("[CUsbEnet/Main] Using fixed %u g_NicEmac=%p title MAC address=%p value=%02X:%02X:%02X:%02X:%02X:%02X; no debug XNet instance.\n", KernelBuild, reinterpret_cast<PVOID>(Kernel->NicEmac), TitleAddress, TitleAddress->_ab[0], TitleAddress->_ab[1], TitleAddress->_ab[2], TitleAddress->_ab[3], TitleAddress->_ab[4], TitleAddress->_ab[5]);
        }

        DbgPrint("[CUsbEnet/Main] Resolving CNicBase addresses\n");
        CNicBase::ResolveFunctions();

        DbgPrint("[CUsbEnet/Main] Calling g_UsbEnet.DriverEntry() object=%p\n", &g_UsbEnet);
        g_UsbEnet.DriverEntry();

        DbgPrint("[CUsbEnet/Main] Installing Hooks\n");
        Hooks::Install();

        DbgPrint("[CUsbEnet/Main] Registering driver object=%p\n", &g_EthernetUsbdDriverObject);
        NTSTATUS driverResult = UsbdRegisterDriverObject(&g_EthernetUsbdDriverObject);
        DbgPrint("[CUsbEnet/Main] UsbdRegisterDriverObject result=0x%08X links=%p/%p.\n", driverResult, g_EthernetUsbdDriverObject.RegisteredDriverListEntry.Flink, g_EthernetUsbdDriverObject.RegisteredDriverListEntry.Blink);
        if (NT_SUCCESS(driverResult)) {
            InterlockedExchange(&DriverRegistered, 1);
            DWORD ConnectCount = 0;
            driverResult = ReenumerateUnrecognizedUsbDevices(&ConnectCount, TRUE);
            if (!NT_SUCCESS(driverResult)) {
                DbgPrint("[CUsbEnet/Main] USB re-enumeration failed: 0x%08X\n", driverResult);
            } else if (g_UsbEnet.DeviceAttached == 0) {
                DbgPrint("[CUsbEnet/Main] Starting deferred USB re-enumeration retries.\n");
                Utilities::CreateThread(UsbReenumerationRetryThread, NULL);
            }
        }
        InterlockedExchange(&InitState, 2);
        Status::Start();
        DbgPrint("[CUsbEnet/Main] Init complete.\n");
    }

    VOID Shutdown() {
        if (InitState != 2) {
            DbgPrint("[CUsbEnet/Main] Shutdown skipped because initialization state=%ld.\n", InitState);
            return;
        }

        Status::Stop();

        PUSBD_DEVICE_NODE DeviceNode = g_UsbEnet.DeviceAttached != 0 ? g_UsbEnet.DeviceNode : NULL;
        DbgPrint("[CUsbEnet/Main] Removing hooks.\n");
        Hooks::Remove();
        UsbEnetFlushXmitCompletions();
        if (InterlockedCompareExchange(&DriverRegistered, 0, 1) == 1) {
            PLIST_ENTRY Flink = g_EthernetUsbdDriverObject.RegisteredDriverListEntry.Flink;
            PLIST_ENTRY Blink = g_EthernetUsbdDriverObject.RegisteredDriverListEntry.Blink;

            if (Flink != NULL && Blink != NULL && MmIsAddressValid(Flink) && MmIsAddressValid(Blink) && Flink->Blink == &g_EthernetUsbdDriverObject.RegisteredDriverListEntry && Blink->Flink == &g_EthernetUsbdDriverObject.RegisteredDriverListEntry) {
                DbgPrint("[CUsbEnet/Main] Unregistering linked driver object links=%p/%p.\n", Flink, Blink);
                UsbdUnregisterDriverObject(&g_EthernetUsbdDriverObject);
            } else {
                DbgPrint("[CUsbEnet/Main] Skipping unsafe USBD unregister links=%p/%p.\n", Flink, Blink);
            }
        }

        if (DeviceNode != NULL)
            SetUsbDeviceUnrecognized(DeviceNode);

        if (g_UsbEnet.DeviceAttached != 0) {
            DbgPrint("[CUsbEnet/Main] Removing device extension.\n");
            g_UsbEnet.BeginRemoveDeviceExtension(&g_UsbEnet);
        }

        DbgPrint("[CUsbEnet/Main] Shutdown complete.\n");
        InterlockedExchange(&InitState, 0);
    }

    static DWORD WINAPI InitThread(LPVOID) {
        Init();
        return 0;
    }
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        Main::mHandle = hModule;
        if (XamLoaderGetDvdTrayState() != DVD_TRAY_STATE_OPEN && InterlockedCompareExchange(&Main::InitState, 1, 0) == 0) {
            DbgPrint("[CUsbEnet/Main] Queueing asynchronous initialization.\n");
            Utilities::CreateThread(Main::InitThread, NULL);
        }
        break;
    case DLL_PROCESS_DETACH:
        Main::Exiting = TRUE;
        Main::Shutdown();
        break;
    }
    return TRUE;
}
