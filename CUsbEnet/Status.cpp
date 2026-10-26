#include "stdafx.h"
#include "UsbEnetChipset.h"

USBENET_DEVICE_INFO g_UsbEnetLastDevice;
USBENET_STATS_SNAPSHOT g_UsbEnetStatsSnapshot;

namespace Status {
    static const char StatusFileName[] = "NetUsb360-status.txt";
    // XAM can't show a message this early in boot, so wait before the first one.
    static const DWORD NotifyEarliestMilliseconds = 15000;
    // How long to wait for a link before reporting that there isn't one.
    static const DWORD NotifyGiveUpMilliseconds = 45000;
    static const DWORD StatusFilePeriodMilliseconds = 5000;
    static const DWORD PollMilliseconds = 250;

    static volatile LONG ThreadRunning;

    static DWORD LinkSpeedMbps(DWORD LinkState) {
        if ((LinkState & NIC_LINK_STATE_1000_MBPS) != 0)
            return 1000;
        if ((LinkState & NIC_LINK_STATE_100_MBPS) != 0)
            return 100;
        if ((LinkState & NIC_LINK_STATE_10_MBPS) != 0)
            return 10;
        return 0;
    }

    static const char* AdapterName() {
        return g_UsbEnet.DeviceAttached != 0 && g_UsbEnetChipset != NULL ? g_UsbEnetChipset->GetName() : NULL;
    }

    static VOID Notify(XNOTIFYQUEUEUI_TYPE Type, const WCHAR* Text) {
        USBENET_INFO_PRINT("[NetUsb360] %S\n", Text);
        XNotifyQueueUI(Type, 0, XNOTIFYUI_PRIORITY_HIGH, const_cast<PWCHAR>(Text), NULL);
    }

    /* Returns TRUE once a message has been shown: the link is up, or the
     * give-up time passed without one and the message says why. */
    static BOOL TryNotify(DWORD ElapsedMilliseconds) {
        WCHAR Text[128];
        const char* Name = AdapterName();
        DWORD LinkState = Name != NULL ? g_UsbEnet.LinkState : 0;

        if (ElapsedMilliseconds < NotifyEarliestMilliseconds)
            return FALSE;

        if ((LinkState & NIC_LINK_STATE_ACTIVE) != 0) {
            swprintf_s(Text, ARRAYSIZE(Text), L"NetUsb360: %S connected at %u Mbit/s", Name, LinkSpeedMbps(LinkState));
            Notify(XNOTIFYUI_TYPE_PREFERRED_REVIEW, Text);
            return TRUE;
        }

        if (ElapsedMilliseconds < NotifyGiveUpMilliseconds)
            return FALSE;

        if (Name != NULL)
            swprintf_s(Text, ARRAYSIZE(Text), L"NetUsb360: %S found, but no network link. Check the cable.", Name);
        else if (g_UsbEnetLastDevice.Seen != 0 && !g_UsbEnetLastDevice.Supported)
            swprintf_s(Text, ARRAYSIZE(Text), L"NetUsb360: USB adapter %04X:%04X is not supported", g_UsbEnetLastDevice.VendorId, g_UsbEnetLastDevice.ProductId);
        else
            swprintf_s(Text, ARRAYSIZE(Text), L"NetUsb360: no supported USB Ethernet adapter found");

        Notify(XNOTIFYUI_TYPE_AVOID_REVIEW, Text);
        return TRUE;
    }

    static VOID WriteStatusFile() {
        char Text[1024];
        const char* Name = AdapterName();
        DWORD LinkState = Name != NULL ? g_UsbEnet.LinkState : 0;
        USBENET_STATS_SNAPSHOT Stats = g_UsbEnetStatsSnapshot;
        int Length = 0;

        Length += sprintf_s(Text + Length, sizeof(Text) - Length, "NetUsb360 %d.%d.%d (%s, %s build)\r\nKernel: %u %s\r\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, USBENET_BUILD_VARIANT, USBENET_BUILD_KIND, Main::KernelBuild, Main::Devkit ? "devkit" : "retail");

        if (Name != NULL)
            Length += sprintf_s(Text + Length, sizeof(Text) - Length, "Adapter: %s (%04X:%04X rev %04X)\r\n", Name, g_UsbEnetLastDevice.VendorId, g_UsbEnetLastDevice.ProductId, g_UsbEnetLastDevice.Revision);
        else if (g_UsbEnetLastDevice.Seen != 0)
            Length += sprintf_s(Text + Length, sizeof(Text) - Length, "Adapter: none attached (last USB device offered: %04X:%04X rev %04X, %s)\r\n", g_UsbEnetLastDevice.VendorId, g_UsbEnetLastDevice.ProductId, g_UsbEnetLastDevice.Revision, g_UsbEnetLastDevice.Supported ? "supported" : "not supported");
        else
            Length += sprintf_s(Text + Length, sizeof(Text) - Length, "Adapter: none found\r\n");

        if ((LinkState & NIC_LINK_STATE_ACTIVE) != 0)
            Length += sprintf_s(Text + Length, sizeof(Text) - Length, "Link: up, %u Mbit/s, %s duplex\r\n", LinkSpeedMbps(LinkState), (LinkState & NIC_LINK_STATE_HALF_DUPLEX) != 0 ? "half" : "full");
        else
            Length += sprintf_s(Text + Length, sizeof(Text) - Length, "Link: down\r\n");

        Length += sprintf_s(Text + Length, sizeof(Text) - Length, "Settings: file %s, Ax88179TxAggregation=%u CoalesceSmallFrames=%u AggregateSmallOnly=%u Notify=%u StatusFile=%u\r\n", g_UsbEnetSettings.FileFound ? "found" : "not found", g_UsbEnetSettings.Ax88179TxAggregation, g_UsbEnetSettings.CoalesceSmallFrames, g_UsbEnetSettings.AggregateSmallOnly, g_UsbEnetSettings.Notify, g_UsbEnetSettings.StatusFile);

        if (Stats.Sequence != 0)
            Length += sprintf_s(Text + Length, sizeof(Text) - Length, "Last %u ms: to Xbox %u.%02u Mbit/s (%u frames), from Xbox %u.%02u Mbit/s (%u frames), send drops %u, send errors %u, receive errors %u\r\n", Stats.ElapsedMilliseconds, Stats.RxFrameMbpsX100 / 100, Stats.RxFrameMbpsX100 % 100, Stats.RxFrames, Stats.TxFrameMbpsX100 / 100, Stats.TxFrameMbpsX100 % 100, Stats.TxFrames, Stats.TxDrops, Stats.TxErrors, Stats.RxErrors);

        if (Length > 0)
            Settings::WriteTextFile(StatusFileName, Text, static_cast<DWORD>(Length));
    }

    static DWORD WINAPI StatusThread(LPVOID) {
        DWORD StartTick = GetTickCount();
        DWORD LastWriteTick = 0;
        BOOL Notified = !g_UsbEnetSettings.Notify;
        BOOL Written = FALSE;

        while (!Main::Exiting) {
            DWORD Now = GetTickCount();

            if (!Notified)
                Notified = TryNotify(Now - StartTick);

            if (g_UsbEnetSettings.StatusFile && (!Written || Now - LastWriteTick >= StatusFilePeriodMilliseconds)) {
                WriteStatusFile();
                LastWriteTick = Now;
                Written = TRUE;
            }

            if (Notified && !g_UsbEnetSettings.StatusFile)
                break;

            Sleep(PollMilliseconds);
        }

        InterlockedExchange(&ThreadRunning, 0);
        return 0;
    }

    VOID Start() {
        if (!g_UsbEnetSettings.Notify && !g_UsbEnetSettings.StatusFile)
            return;

        if (InterlockedCompareExchange(&ThreadRunning, 1, 0) == 0)
            Utilities::CreateThread(StatusThread, NULL);
    }

    VOID Stop() {
        // The thread checks Main::Exiting every PollMilliseconds; a status
        // file write can take longer, so allow a few seconds.
        for (DWORD Waited = 0; ThreadRunning != 0 && Waited < 5000; Waited += 50)
            Sleep(50);
    }
}
