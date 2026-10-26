#pragma once

// The USB device the driver was last offered, normalized to the usual byte
// order. Lets the on-screen message and the status file name an adapter the
// driver doesn't support.
typedef struct _USBENET_DEVICE_INFO {
    volatile LONG Seen;
    USHORT VendorId;
    USHORT ProductId;
    USHORT Revision;
    BOOL Supported;
} USBENET_DEVICE_INFO, *PUSBENET_DEVICE_INFO;

// Totals for the last throughput period (ThroughputStatsPeriod), copied by
// PrintThroughputStats in every build. Sequence changes after each copy.
typedef struct _USBENET_STATS_SNAPSHOT {
    volatile LONG Sequence;
    DWORD ElapsedMilliseconds;
    DWORD TxFrameMbpsX100;
    DWORD RxFrameMbpsX100;
    DWORD TxFrames;
    DWORD RxFrames;
    DWORD TxDrops;
    DWORD TxErrors;
    DWORD RxErrors;
} USBENET_STATS_SNAPSHOT, *PUSBENET_STATS_SNAPSHOT;

extern USBENET_DEVICE_INFO g_UsbEnetLastDevice;
extern USBENET_STATS_SNAPSHOT g_UsbEnetStatsSnapshot;

namespace Status {
    // Starts the thread behind the on-screen message and the status file, if
    // either is turned on. Stop waits for that thread before the plugin unloads.
    VOID Start();
    VOID Stop();
}
