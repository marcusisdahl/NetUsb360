#pragma once

// Options read from NetUsb360.ini next to the plugin when the driver loads.
// Each one starts at its build-switch default (see Main.h), so a missing or
// empty file behaves exactly like the build without a settings file.
typedef struct _USBENET_SETTINGS {
    BOOL Ax88179TxAggregation;      // USBENET_AX88179_TX_AGGREGATION
    BOOL CoalesceSmallFrames;       // USBENET_COALESCE_TITLE_SMALL_FRAMES
    BOOL AggregateSmallOnly;        // USBENET_TX_AGGREGATE_SMALL_ONLY
    BOOL Notify;                    // On-screen message once the adapter is ready
    BOOL StatusFile;                // Keep NetUsb360-status.txt up to date
    BOOL FileFound;
} USBENET_SETTINGS, *PUSBENET_SETTINGS;

extern USBENET_SETTINGS g_UsbEnetSettings;

namespace Settings {
    // Call once from Init, at PASSIVE_LEVEL, before the driver registers.
    VOID Load(HANDLE Module);
    // Replaces a file in the plugin's folder. PASSIVE_LEVEL only.
    BOOL WriteTextFile(const char* FileName, const char* Text, DWORD Length);
    const char* GetDirectory();
}
