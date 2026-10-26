#include "stdafx.h"

USBENET_SETTINGS g_UsbEnetSettings = {
    USBENET_AX88179_TX_AGGREGATION != 0,
    USBENET_COALESCE_TITLE_SMALL_FRAMES != 0,
    USBENET_TX_AGGREGATE_SMALL_ONLY != 0,
    TRUE,
    FALSE,
    FALSE
};

namespace Settings {
    static const char SettingsFileName[] = "NetUsb360.ini";
    static const char FallbackDirectory[] = "\\Device\\Harddisk0\\Partition1";
    static const DWORD MaximumSettingsFileSize = 4096;
    static char Directory[MAX_PATH];

    const char* GetDirectory() {
        return Directory;
    }

    /* The loader entry names the XEX by its device path, for example
     * \Device\Harddisk0\Partition1\Plugins\NetUsb360.xex. Keep the folder so
     * the settings file can sit next to the plugin. Fall back to the root of
     * the internal hard drive when the path has another form. */
    static VOID FindDirectory(HANDLE Module) {
        PLDR_DATA_TABLE_ENTRY Entry = static_cast<PLDR_DATA_TABLE_ENTRY>(Module);
        DWORD LastSeparator = 0;
        DWORD Count = 0;

        strcpy_s(Directory, sizeof(Directory), FallbackDirectory);

        if (Entry == NULL || !MmIsAddressValid(Entry) || Entry->FullDllName.Buffer == NULL || !MmIsAddressValid(Entry->FullDllName.Buffer))
            return;

        Count = Entry->FullDllName.Length / sizeof(WCHAR);
        if (Count == 0 || Count >= sizeof(Directory))
            return;

        char Path[MAX_PATH];
        for (DWORD Index = 0; Index < Count; Index++) {
            WCHAR Character = Entry->FullDllName.Buffer[Index];

            if (Character == 0 || Character > 0x7F)
                return;

            Path[Index] = static_cast<char>(Character);
            if (Character == L'\\')
                LastSeparator = Index;
        }
        Path[Count] = '\0';

        if (_strnicmp(Path, "\\Device\\", 8) != 0 || LastSeparator <= 8)
            return;

        Path[LastSeparator] = '\0';
        strcpy_s(Directory, sizeof(Directory), Path);
    }

    static NTSTATUS OpenFile(const char* FileName, BOOL Write, PHANDLE Handle) {
        char Path[MAX_PATH];
        STRING ObjectName;
        OBJECT_ATTRIBUTES Attributes;
        IO_STATUS_BLOCK IoStatus;

        if (sprintf_s(Path, sizeof(Path), "%s\\%s", Directory, FileName) < 0)
            return STATUS_OBJECT_NAME_INVALID;

        ObjectName.Buffer = Path;
        ObjectName.Length = static_cast<USHORT>(strlen(Path));
        ObjectName.MaximumLength = ObjectName.Length + 1;
        InitializeObjectAttributes(&Attributes, &ObjectName, OBJ_CASE_INSENSITIVE, NULL);

        return NtCreateFile(Handle, Write ? (GENERIC_WRITE | SYNCHRONIZE) : (GENERIC_READ | SYNCHRONIZE), &Attributes, &IoStatus, NULL, FILE_ATTRIBUTE_NORMAL, Write ? 0 : FILE_SHARE_READ, Write ? FILE_OVERWRITE_IF : FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE);
    }

    BOOL WriteTextFile(const char* FileName, const char* Text, DWORD Length) {
        HANDLE Handle = NULL;
        IO_STATUS_BLOCK IoStatus;

        if (KeGetCurrentIrql() != PASSIVE_LEVEL || !NT_SUCCESS(OpenFile(FileName, TRUE, &Handle)))
            return FALSE;

        NTSTATUS Status = NtWriteFile(Handle, NULL, NULL, NULL, &IoStatus, const_cast<char*>(Text), Length, NULL);
        NtClose(Handle);
        return NT_SUCCESS(Status);
    }

    static char* Trim(char* Text) {
        while (*Text == ' ' || *Text == '\t')
            ++Text;

        char* End = Text + strlen(Text);
        while (End > Text && (End[-1] == ' ' || End[-1] == '\t' || End[-1] == '\r'))
            *--End = '\0';

        return Text;
    }

    static BOOL ParseBoolean(const char* Value, PBOOL Result) {
        if (strcmp(Value, "1") == 0 || _stricmp(Value, "true") == 0 || _stricmp(Value, "yes") == 0 || _stricmp(Value, "on") == 0) {
            *Result = TRUE;
            return TRUE;
        }

        if (strcmp(Value, "0") == 0 || _stricmp(Value, "false") == 0 || _stricmp(Value, "no") == 0 || _stricmp(Value, "off") == 0) {
            *Result = FALSE;
            return TRUE;
        }

        return FALSE;
    }

    /* One "Key = Value" per line. Lines starting with ; or # and [section]
     * lines are ignored, as is anything after ; or # on a line. When a key
     * appears twice the later line wins, so Acksmall can be followed by one
     * of the separate keys to override it. */
    static VOID ApplyLine(char* Line, DWORD LineNumber) {
        char* Comment = strpbrk(Line, ";#");
        if (Comment != NULL)
            *Comment = '\0';

        Line = Trim(Line);
        if (*Line == '\0' || *Line == '[')
            return;

        char* Separator = strchr(Line, '=');
        if (Separator == NULL) {
            USBENET_INFO_PRINT("[NetUsb360] %s line %u: no '=', ignored.\n", SettingsFileName, LineNumber);
            return;
        }

        *Separator = '\0';
        char* Key = Trim(Line);
        char* Text = Trim(Separator + 1);
        BOOL Value = FALSE;

        if (!ParseBoolean(Text, &Value)) {
            USBENET_INFO_PRINT("[NetUsb360] %s line %u: %s needs 0 or 1, ignored.\n", SettingsFileName, LineNumber, Key);
            return;
        }

        if (_stricmp(Key, "Acksmall") == 0) {
            g_UsbEnetSettings.Ax88179TxAggregation = Value;
            g_UsbEnetSettings.CoalesceSmallFrames = Value;
            g_UsbEnetSettings.AggregateSmallOnly = Value;
        } else if (_stricmp(Key, "Ax88179TxAggregation") == 0) {
            g_UsbEnetSettings.Ax88179TxAggregation = Value;
        } else if (_stricmp(Key, "CoalesceSmallFrames") == 0) {
            g_UsbEnetSettings.CoalesceSmallFrames = Value;
        } else if (_stricmp(Key, "AggregateSmallOnly") == 0) {
            g_UsbEnetSettings.AggregateSmallOnly = Value;
        } else if (_stricmp(Key, "Notify") == 0) {
            g_UsbEnetSettings.Notify = Value;
        } else if (_stricmp(Key, "StatusFile") == 0) {
            g_UsbEnetSettings.StatusFile = Value;
        } else {
            USBENET_INFO_PRINT("[NetUsb360] %s line %u: unknown setting %s, ignored.\n", SettingsFileName, LineNumber, Key);
        }
    }

    VOID Load(HANDLE Module) {
        static char Buffer[MaximumSettingsFileSize + 1];
        HANDLE Handle = NULL;
        IO_STATUS_BLOCK IoStatus;

        FindDirectory(Module);

        NTSTATUS Status = OpenFile(SettingsFileName, FALSE, &Handle);
        if (!NT_SUCCESS(Status)) {
            USBENET_INFO_PRINT("[NetUsb360] No %s in %s (0x%08X); using the build defaults.\n", SettingsFileName, Directory, Status);
            return;
        }

        Status = NtReadFile(Handle, NULL, NULL, NULL, &IoStatus, Buffer, MaximumSettingsFileSize, NULL);
        NtClose(Handle);

        if (!NT_SUCCESS(Status)) {
            USBENET_INFO_PRINT("[NetUsb360] Could not read %s (0x%08X); using the build defaults.\n", SettingsFileName, Status);
            return;
        }

        DWORD Length = static_cast<DWORD>(IoStatus.Information);
        if (Length > MaximumSettingsFileSize)
            Length = MaximumSettingsFileSize;
        Buffer[Length] = '\0';
        g_UsbEnetSettings.FileFound = TRUE;

        char* Line = Buffer;
        for (DWORD LineNumber = 1; Line != NULL && *Line != '\0'; LineNumber++) {
            char* Next = strchr(Line, '\n');
            if (Next != NULL)
                *Next++ = '\0';

            ApplyLine(Line, LineNumber);
            Line = Next;
        }

        USBENET_INFO_PRINT("[NetUsb360] Read %s: Ax88179TxAggregation=%u CoalesceSmallFrames=%u AggregateSmallOnly=%u Notify=%u StatusFile=%u.\n", SettingsFileName, g_UsbEnetSettings.Ax88179TxAggregation, g_UsbEnetSettings.CoalesceSmallFrames, g_UsbEnetSettings.AggregateSmallOnly, g_UsbEnetSettings.Notify, g_UsbEnetSettings.StatusFile);
    }
}
