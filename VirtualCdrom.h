#pragma once

#include <ntddk.h>
#include <ntddcdrm.h>
#include <ntddscsi.h>
#include <ntdddisk.h>
#include <ntstrsafe.h>
#include <scsi.h>

#define VCD_POOL_TAG 'dcvV'
#define VCD_DEVICE_NAME L"\\Device\\VirtualCdrom"
#define VCD_SYMLINK_NAME L"\\DosDevices\\VirtualCdrom"
#define VCD_ISO_PATH L"\\??\\C:\\Program Files (x86)\\CDROM\\vCD.iso"

#define SECTOR_SIZE 2048
#define CHECK_INTERVAL_MS 10000

typedef enum _VCD_STATE {
    VCD_STATE_NO_MEDIA = 0,
    VCD_STATE_MEDIA_INSERTED,
    VCD_STATE_MEDIA_EJECTED
} VCD_STATE;

typedef struct _VCD_DEVICE_EXTENSION {
    PDEVICE_OBJECT DeviceObject;
    PDEVICE_OBJECT LowerDeviceObject;
    PDEVICE_OBJECT Pdo;
    
    UNICODE_STRING IsoFilePath;
    HANDLE IsoFileHandle;
    PFILE_OBJECT IsoFileObject;
    LARGE_INTEGER IsoFileSize;
    ULONG TotalSectors;
    
    VCD_STATE State;
    BOOLEAN MediaChangeEnabled;
    BOOLEAN DeviceRestarted;
    
    KTIMER CheckTimer;
    KDPC CheckDpc;
    KSPIN_LOCK StateLock;
    
    SCSI_INQUIRY_DATA InquiryData;
    CDROM_TOC CdromToc;
    
} VCD_DEVICE_EXTENSION, *PVCD_DEVICE_EXTENSION;

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD VcdUnload;
DRIVER_DISPATCH VcdCreate;
DRIVER_DISPATCH VcdClose;
DRIVER_DISPATCH VcdRead;
DRIVER_DISPATCH VcdWrite;
DRIVER_DISPATCH VcdDeviceControl;
DRIVER_DISPATCH VcdPnp;
DRIVER_DISPATCH VcdPower;
DRIVER_DISPATCH VcdSystemControl;

IO_COMPLETION_ROUTINE VcdPnpCompletion;

KDEFERRED_ROUTINE VcdCheckTimerDpc;

NTSTATUS VcdCreateDevice(
    _In_ PDRIVER_OBJECT DriverObject,
    _Out_ PDEVICE_OBJECT* DeviceObject
);

NTSTATUS VcdInitializeDevice(
    _In_ PDEVICE_OBJECT DeviceObject
);

VOID VcdCleanupDevice(
    _In_ PDEVICE_OBJECT DeviceObject
);

NTSTATUS VcdOpenIsoFile(
    _In_ PVCD_DEVICE_EXTENSION DevExt
);

VOID VcdCloseIsoFile(
    _In_ PVCD_DEVICE_EXTENSION DevExt
);

NTSTATUS VcdReadIsoSectors(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ ULONG StartSector,
    _In_ ULONG SectorCount,
    _Out_ PVOID Buffer
);

VOID VcdStartCheckTimer(
    _In_ PVCD_DEVICE_EXTENSION DevExt
);

VOID VcdStopCheckTimer(
    _In_ PVCD_DEVICE_EXTENSION DevExt
);

VOID VcdCheckTimerDpc(
    _In_ struct _KDPC* Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
);

NTSTATUS VcdHandleScsiCommand(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ PSCSI_PASS_THROUGH Srb,
    _In_ PIRP Irp
);

NTSTATUS VcdHandleInquiry(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ PSCSI_PASS_THROUGH Srb,
    _In_ PINQUIRYDATA InquiryBuffer,
    _In_ ULONG BufferLength
);

NTSTATUS VcdHandleReadCapacity(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ PREAD_CAPACITY_DATA ReadCapacityBuffer,
    _In_ ULONG BufferLength
);

NTSTATUS VcdHandleReadToc(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ PCDROM_TOC TocBuffer,
    _In_ ULONG BufferLength
);

NTSTATUS VcdHandleReadSectors(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ ULONG StartSector,
    _In_ USHORT SectorCount,
    _Out_ PVOID Buffer,
    _In_ ULONG BufferLength
);

NTSTATUS VcdHandleStartStopUnit(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ BOOLEAN Start,
    _In_ BOOLEAN LoadEject
);

NTSTATUS VcdHandleModeSense(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ PMODE_PARAMETER_HEADER ModeBuffer,
    _In_ ULONG BufferLength
);

VOID VcdInitializeInquiryData(
    _In_ PVCD_DEVICE_EXTENSION DevExt
);

VOID VcdInitializeToc(
    _In_ PVCD_DEVICE_EXTENSION DevExt
);

NTSTATUS VcdInsertMedia(
    _In_ PVCD_DEVICE_EXTENSION DevExt
);

NTSTATUS VcdEjectMedia(
    _In_ PVCD_DEVICE_EXTENSION DevExt
);

BOOLEAN VcdCheckIsoFileExists(
    _In_ PVCD_DEVICE_EXTENSION DevExt
);
