#include "stdafx.h"
#include "UsbEnetChipset.h"
#include "Ax88178.h"
#include "Ax88179x_178A_772D.h"
#include "Rtl8153.h"

CUsbEnetChipset* g_UsbEnetChipset = &g_Ax88178Chipset;

NTSTATUS CUsbEnetChipset::RequestFullReinitialize(CUsbEnet* Device, DWORD Reason) {
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Reason);
    return STATUS_NOT_SUPPORTED;
}

VOID CUsbEnetChipset::HandleTransportSubmission(CUsbEnet* Device, USBENET_TRANSPORT_PIPE Pipe) {
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Pipe);
}

VOID CUsbEnetChipset::HandleTransportCompletion(CUsbEnet* Device, USBENET_TRANSPORT_PIPE Pipe, NTSTATUS Status) {
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Pipe);
    UNREFERENCED_PARAMETER(Status);
}

BOOL CUsbEnetChipset::UsesInterruptLinkStatus() const {
    return FALSE;
}

BOOL CUsbEnetChipset::ProcessInterruptLinkStatus(CUsbEnet* Device, const BYTE* Data, DWORD Length) {
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(Length);
    return FALSE;
}

// USB IDs are stored as read on PPC: byte-swapped, so 0BDA:8153 is 0xDA0B/0x5381.
#define USBENET_RAW_ID(_id) static_cast<USHORT>((((_id) & 0xFF) << 8) | (((_id) >> 8) & 0xFF))

typedef struct _USBENET_ADAPTER_ID {
    USHORT VendorIdRaw;
    USHORT ProductIdRaw;
} USBENET_ADAPTER_ID;

/* RTL8153 adapters sold under other brands, from the Linux r8152 device
 * table. The driver still reads the chip's hardware version and refuses ones
 * it doesn't support, so an entry here that turns out to be another Realtek
 * chip fails cleanly. */
static const USBENET_ADAPTER_ID Rtl8153AdapterIds[] = {
    { USBENET_RAW_ID(0x2357), USBENET_RAW_ID(0x0601) }, // TP-Link UE300
    { USBENET_RAW_ID(0x045E), USBENET_RAW_ID(0x07AB) }, // Microsoft Surface Ethernet Adapter
    { USBENET_RAW_ID(0x045E), USBENET_RAW_ID(0x07C6) }, // Microsoft Surface Dock
    { USBENET_RAW_ID(0x04E8), USBENET_RAW_ID(0xA101) }, // Samsung
    { USBENET_RAW_ID(0x13B1), USBENET_RAW_ID(0x0041) }, // Linksys USB3GIGV1
    { USBENET_RAW_ID(0x0955), USBENET_RAW_ID(0x09FF) }, // NVIDIA SHIELD adapter
    { USBENET_RAW_ID(0x17EF), USBENET_RAW_ID(0x3062) }, // Lenovo ThinkPad USB-C dongle
    { USBENET_RAW_ID(0x17EF), USBENET_RAW_ID(0x3069) }, // Lenovo ThinkPad TBT3 dongle
    { USBENET_RAW_ID(0x17EF), USBENET_RAW_ID(0x7205) }, // Lenovo OneLink+ dock
    { USBENET_RAW_ID(0x17EF), USBENET_RAW_ID(0x720C) }, // Lenovo USB-C dock
    { USBENET_RAW_ID(0x17EF), USBENET_RAW_ID(0x7214) }  // Lenovo USB-C travel hub
};

/* AX88179 adapters sold under other brands, from the Linux ax88179_178a
 * device table. They report the same bcdDevice revisions as ASIX's own ID. */
static const USBENET_ADAPTER_ID Ax88179AdapterIds[] = {
    { USBENET_RAW_ID(0x0B95), USBENET_RAW_ID(0x1790) }, // ASIX
    { USBENET_RAW_ID(0x0DF6), USBENET_RAW_ID(0x0072) }, // Sitecom
    { USBENET_RAW_ID(0x04E8), USBENET_RAW_ID(0xA100) }, // Samsung
    { USBENET_RAW_ID(0x17EF), USBENET_RAW_ID(0x304B) }, // Lenovo
    { USBENET_RAW_ID(0x0930), USBENET_RAW_ID(0x0A13) }  // Toshiba
};

static BOOL UsbEnetIsListedAdapter(const USBENET_ADAPTER_ID* Ids, DWORD Count, USHORT VendorIdRaw, USHORT ProductIdRaw) {
    for (DWORD Index = 0; Index < Count; Index++) {
        if (Ids[Index].VendorIdRaw == VendorIdRaw && Ids[Index].ProductIdRaw == ProductIdRaw)
            return TRUE;
    }

    return FALSE;
}

CUsbEnetChipset* UsbEnetFindChipset(USHORT VendorIdRaw, USHORT ProductIdRaw, USHORT DeviceRevision) {
    if (VendorIdRaw == g_Rtl8153Chipset.GetVendorIdRaw() && ProductIdRaw == g_Rtl8153Chipset.GetProductIdRaw())
        return &g_Rtl8153Chipset;

    if (UsbEnetIsListedAdapter(Rtl8153AdapterIds, ARRAYSIZE(Rtl8153AdapterIds), VendorIdRaw, ProductIdRaw))
        return &g_Rtl8153Chipset;

    if (UsbEnetIsListedAdapter(Ax88179AdapterIds, ARRAYSIZE(Ax88179AdapterIds), VendorIdRaw, ProductIdRaw)) {
        switch (DeviceRevision) {
            case 0x0100: return &g_Ax88179Chipset;
            case 0x0200: return &g_Ax88179ABChipset;
            case 0x0300: return &g_Ax88772DChipset;
            case 0x0400: return &g_Ax88279Chipset;
            default: return NULL;
        }
    }

    CUsbEnetChipset* Chipsets[] = {
        &g_Ax88178Chipset,
        &g_Ax88178AChipset
    };

    for (DWORD Index = 0; Index < ARRAYSIZE(Chipsets); Index++) {
        if (Chipsets[Index]->GetVendorIdRaw() == VendorIdRaw && Chipsets[Index]->GetProductIdRaw() == ProductIdRaw)
            return Chipsets[Index];
    }

    return NULL;
}
