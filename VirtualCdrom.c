#include "VirtualCdrom.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, VcdUnload)
#pragma alloc_text(PAGE, VcdCreate)
#pragma alloc_text(PAGE, VcdClose)
#pragma alloc_text(PAGE, VcdDeviceControl)
#pragma alloc_text(PAGE, VcdInitializeDevice)
#pragma alloc_text(PAGE, VcdCleanupDevice)
#pragma alloc_text(PAGE, VcdOpenIsoFile)
#pragma alloc_text(PAGE, VcdCloseIsoFile)
#pragma alloc_text(PAGE, VcdInsertMedia)
#pragma alloc_text(PAGE, VcdEjectMedia)
#pragma alloc_text(PAGE, VcdHandleScsiCommand)
#pragma alloc_text(PAGE, VcdHandleInquiry)
#pragma alloc_text(PAGE, VcdHandleReadCapacity)
#pragma alloc_text(PAGE, VcdHandleReadToc)
#pragma alloc_text(PAGE, VcdHandleModeSense)
#pragma alloc_text(PAGE, VcdHandleStartStopUnit)
#pragma alloc_text(PAGE, VcdInitializeInquiryData)
#pragma alloc_text(PAGE, VcdInitializeToc)
#endif

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject = NULL;
    
    UNREFERENCED_PARAMETER(RegistryPath);
    
    KdPrint(("VirtualCdrom: DriverEntry\n"));
    
    DriverObject->DriverUnload = VcdUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = VcdCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = VcdClose;
    DriverObject->MajorFunction[IRP_MJ_READ] = VcdRead;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = VcdWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = VcdDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_PNP] = VcdPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER] = VcdPower;
    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = VcdSystemControl;
    
    status = VcdCreateDevice(DriverObject, &deviceObject);
    if (!NT_SUCCESS(status)) {
        KdPrint(("VirtualCdrom: Failed to create device: 0x%08X\n", status));
        return status;
    }
    
    status = VcdInitializeDevice(deviceObject);
    if (!NT_SUCCESS(status)) {
        KdPrint(("VirtualCdrom: Failed to initialize device: 0x%08X\n", status));
        IoDeleteDevice(deviceObject);
        return status;
    }
    
    KdPrint(("VirtualCdrom: Driver loaded successfully\n"));
    return STATUS_SUCCESS;
}

VOID
VcdUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    PDEVICE_OBJECT deviceObject;
    PVCD_DEVICE_EXTENSION devExt;
    
    PAGED_CODE();
    
    KdPrint(("VirtualCdrom: Unload\n"));
    
    deviceObject = DriverObject->DeviceObject;
    if (deviceObject != NULL) {
        devExt = (PVCD_DEVICE_EXTENSION)deviceObject->DeviceExtension;
        VcdCleanupDevice(deviceObject);
        IoDeleteSymbolicLink(&devExt->IsoFilePath);
        IoDeleteDevice(deviceObject);
    }
}

NTSTATUS
VcdCreateDevice(
    _In_ PDRIVER_OBJECT DriverObject,
    _Out_ PDEVICE_OBJECT* DeviceObject
)
{
    NTSTATUS status;
    UNICODE_STRING deviceName;
    UNICODE_STRING symlinkName;
    PDEVICE_OBJECT deviceObj = NULL;
    
    PAGED_CODE();
    
    RtlInitUnicodeString(&deviceName, VCD_DEVICE_NAME);
    RtlInitUnicodeString(&symlinkName, VCD_SYMLINK_NAME);
    
    status = IoCreateDevice(
        DriverObject,
        sizeof(VCD_DEVICE_EXTENSION),
        &deviceName,
        FILE_DEVICE_CD_ROM,
        FILE_DEVICE_SECURE_OPEN | FILE_READ_ONLY_DEVICE,
        FALSE,
        &deviceObj
    );
    
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    status = IoCreateSymbolicLink(&symlinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObj);
        return status;
    }
    
    *DeviceObject = deviceObj;
    return STATUS_SUCCESS;
}

NTSTATUS
VcdInitializeDevice(
    _In_ PDEVICE_OBJECT DeviceObject
)
{
    PVCD_DEVICE_EXTENSION devExt;
    UNICODE_STRING isoPath;
    
    PAGED_CODE();
    
    devExt = (PVCD_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    RtlZeroMemory(devExt, sizeof(VCD_DEVICE_EXTENSION));
    
    devExt->DeviceObject = DeviceObject;
    devExt->State = VCD_STATE_NO_MEDIA;
    devExt->MediaChangeEnabled = TRUE;
    devExt->DeviceRestarted = TRUE;
    devExt->IsoFileHandle = NULL;
    devExt->IsoFileObject = NULL;
    
    RtlInitUnicodeString(&isoPath, VCD_ISO_PATH);
    NTSTATUS status = RtlCopyUnicodeString(&devExt->IsoFilePath, &isoPath);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    KeInitializeSpinLock(&devExt->StateLock);
    KeInitializeTimer(&devExt->CheckTimer);
    KeInitializeDpc(&devExt->CheckDpc, VcdCheckTimerDpc, devExt);
    
    VcdInitializeInquiryData(devExt);
    VcdInitializeToc(devExt);
    
    VcdStartCheckTimer(devExt);
    
    return STATUS_SUCCESS;
}

VOID
VcdCleanupDevice(
    _In_ PDEVICE_OBJECT DeviceObject
)
{
    PVCD_DEVICE_EXTENSION devExt;
    
    PAGED_CODE();
    
    devExt = (PVCD_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    
    VcdStopCheckTimer(devExt);
    VcdCloseIsoFile(devExt);
    
    if (devExt->IsoFilePath.Buffer != NULL) {
        RtlFreeUnicodeString(&devExt->IsoFilePath);
    }
}

NTSTATUS
VcdCreate(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    PAGED_CODE();
    
    UNREFERENCED_PARAMETER(DeviceObject);
    
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return STATUS_SUCCESS;
}

NTSTATUS
VcdClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    PAGED_CODE();
    
    UNREFERENCED_PARAMETER(DeviceObject);
    
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return STATUS_SUCCESS;
}

NTSTATUS
VcdRead(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    PVCD_DEVICE_EXTENSION devExt;
    PIO_STACK_LOCATION irpStack;
    PUCHAR userBuffer;
    ULONG length;
    LARGE_INTEGER offset;
    NTSTATUS status;
    
    PAGED_CODE();
    
    devExt = (PVCD_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    irpStack = IoGetCurrentIrpStackLocation(Irp);
    
    userBuffer = (PUCHAR)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
    if (userBuffer == NULL) {
        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    length = irpStack->Parameters.Read.Length;
    offset = irpStack->Parameters.Read.ByteOffset;
    
    if (devExt->State != VCD_STATE_MEDIA_INSERTED || devExt->IsoFileHandle == NULL) {
        Irp->IoStatus.Status = STATUS_NO_MEDIA_IN_DEVICE;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_NO_MEDIA_IN_DEVICE;
    }
    
    status = VcdReadIsoSectors(devExt, (ULONG)(offset.QuadPart / SECTOR_SIZE), length / SECTOR_SIZE, userBuffer);
    
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = NT_SUCCESS(status) ? length : 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return status;
}

NTSTATUS
VcdWrite(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    
    Irp->IoStatus.Status = STATUS_MEDIA_WRITE_PROTECTED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return STATUS_MEDIA_WRITE_PROTECTED;
}

NTSTATUS
VcdDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    PVCD_DEVICE_EXTENSION devExt;
    PIO_STACK_LOCATION irpStack;
    NTSTATUS status;
    ULONG ioctl;
    
    PAGED_CODE();
    
    devExt = (PVCD_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    irpStack = IoGetCurrentIrpStackLocation(Irp);
    ioctl = irpStack->Parameters.DeviceIoControl.IoControlCode;
    
    switch (ioctl) {
        case IOCTL_SCSI_PASS_THROUGH:
        case IOCTL_SCSI_PASS_THROUGH_DIRECT:
        {
            PSCSI_PASS_THROUGH srb;
            
            if (irpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(SCSI_PASS_THROUGH)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            
            srb = (PSCSI_PASS_THROUGH)Irp->AssociatedIrp.SystemBuffer;
            status = VcdHandleScsiCommand(devExt, srb, Irp);
            break;
        }
        
        case IOCTL_STORAGE_CHECK_VERIFY:
        case IOCTL_CDROM_CHECK_VERIFY:
        case IOCTL_DISK_CHECK_VERIFY:
        {
            if (devExt->State != VCD_STATE_MEDIA_INSERTED) {
                status = STATUS_NO_MEDIA_IN_DEVICE;
            } else {
                status = STATUS_SUCCESS;
            }
            break;
        }
        
        case IOCTL_STORAGE_GET_MEDIA_TYPES:
        case IOCTL_CDROM_GET_DRIVE_GEOMETRY:
        case IOCTL_DISK_GET_DRIVE_GEOMETRY:
        {
            PDISK_GEOMETRY geometry;
            
            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(DISK_GEOMETRY)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            
            geometry = (PDISK_GEOMETRY)Irp->AssociatedIrp.SystemBuffer;
            geometry->MediaType = CD_ROM;
            geometry->Cylinders.QuadPart = devExt->TotalSectors;
            geometry->TracksPerCylinder = 1;
            geometry->SectorsPerTrack = 1;
            geometry->BytesPerSector = SECTOR_SIZE;
            
            Irp->IoStatus.Information = sizeof(DISK_GEOMETRY);
            status = STATUS_SUCCESS;
            break;
        }
        
        case IOCTL_CDROM_READ_TOC:
        {
            PCDROM_TOC toc;
            
            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(CDROM_TOC)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            
            if (devExt->State != VCD_STATE_MEDIA_INSERTED) {
                status = STATUS_NO_MEDIA_IN_DEVICE;
                break;
            }
            
            toc = (PCDROM_TOC)Irp->AssociatedIrp.SystemBuffer;
            status = VcdHandleReadToc(devExt, toc, irpStack->Parameters.DeviceIoControl.OutputBufferLength);
            
            if (NT_SUCCESS(status)) {
                Irp->IoStatus.Information = sizeof(CDROM_TOC);
            }
            break;
        }
        
        case IOCTL_STORAGE_EJECT_MEDIA:
        case IOCTL_CDROM_EJECT_MEDIA:
        {
            KIRQL oldIrql;
            
            KeAcquireSpinLock(&devExt->StateLock, &oldIrql);
            
            if (devExt->State == VCD_STATE_MEDIA_INSERTED) {
                VcdEjectMedia(devExt);
                devExt->State = VCD_STATE_MEDIA_EJECTED;
            }
            
            KeReleaseSpinLock(&devExt->StateLock, oldIrql);
            
            status = STATUS_SUCCESS;
            break;
        }
        
        case IOCTL_STORAGE_LOAD_MEDIA:
        case IOCTL_CDROM_LOAD_MEDIA:
        {
            status = VcdInsertMedia(devExt);
            break;
        }
        
        default:
        {
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
    }
    
    Irp->IoStatus.Status = status;
    if (Irp->IoStatus.Information == 0) {
        Irp->IoStatus.Information = 0;
    }
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return status;
}

NTSTATUS
VcdPnp(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    PIO_STACK_LOCATION irpStack;
    NTSTATUS status;
    
    irpStack = IoGetCurrentIrpStackLocation(Irp);
    
    switch (irpStack->MinorFunction) {
        case IRP_MN_START_DEVICE:
        case IRP_MN_QUERY_REMOVE_DEVICE:
        case IRP_MN_REMOVE_DEVICE:
        case IRP_MN_CANCEL_REMOVE_DEVICE:
        case IRP_MN_STOP_DEVICE:
        case IRP_MN_QUERY_STOP_DEVICE:
        case IRP_MN_CANCEL_STOP_DEVICE:
        case IRP_MN_SURPRISE_REMOVAL:
        default:
            status = STATUS_SUCCESS;
            break;
    }
    
    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS
VcdPower(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    
    PoStartNextPowerIrp(Irp);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS
VcdSystemControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

VOID
VcdCheckTimerDpc(
    _In_ struct _KDPC* Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
)
{
    PVCD_DEVICE_EXTENSION devExt;
    KIRQL oldIrql;
    BOOLEAN shouldCheck = FALSE;
    
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);
    
    if (DeferredContext == NULL) {
        return;
    }
    
    devExt = (PVCD_DEVICE_EXTENSION)DeferredContext;
    
    KeAcquireSpinLock(&devExt->StateLock, &oldIrql);
    
    if (devExt->State == VCD_STATE_NO_MEDIA && devExt->DeviceRestarted) {
        shouldCheck = TRUE;
    }
    
    KeReleaseSpinLock(&devExt->StateLock, oldIrql);
    
    if (shouldCheck) {
        if (VcdCheckIsoFileExists(devExt)) {
            KdPrint(("VirtualCdrom: ISO file found, inserting media\n"));
            VcdInsertMedia(devExt);
        }
        
        VcdStartCheckTimer(devExt);
    }
}

VOID
VcdStartCheckTimer(
    _In_ PVCD_DEVICE_EXTENSION DevExt
)
{
    LARGE_INTEGER dueTime;
    
    dueTime.QuadPart = -((LONGLONG)CHECK_INTERVAL_MS * 10000LL);
    KeSetTimer(&DevExt->CheckTimer, dueTime, &DevExt->CheckDpc);
}

VOID
VcdStopCheckTimer(
    _In_ PVCD_DEVICE_EXTENSION DevExt
)
{
    KeCancelTimer(&DevExt->CheckTimer);
}

BOOLEAN
VcdCheckIsoFileExists(
    _In_ PVCD_DEVICE_EXTENSION DevExt
)
{
    NTSTATUS status;
    HANDLE fileHandle = NULL;
    OBJECT_ATTRIBUTES objAttr;
    IO_STATUS_BLOCK ioStatus;
    
    InitializeObjectAttributes(&objAttr, &DevExt->IsoFilePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    
    status = ZwCreateFile(
        &fileHandle,
        FILE_READ_DATA | SYNCHRONIZE,
        &objAttr,
        &ioStatus,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ,
        FILE_OPEN,
        FILE_NON_DIRECTORY_FILE,
        NULL,
        0
    );
    
    if (NT_SUCCESS(status)) {
        ZwClose(fileHandle);
        return TRUE;
    }
    
    return FALSE;
}

NTSTATUS
VcdOpenIsoFile(
    _In_ PVCD_DEVICE_EXTENSION DevExt
)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    IO_STATUS_BLOCK ioStatus;
    FILE_STANDARD_INFORMATION fileInfo;
    
    PAGED_CODE();
    
    if (DevExt->IsoFileHandle != NULL) {
        return STATUS_SUCCESS;
    }
    
    InitializeObjectAttributes(&objAttr, &DevExt->IsoFilePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    
    status = ZwCreateFile(
        &DevExt->IsoFileHandle,
        FILE_READ_DATA | SYNCHRONIZE,
        &objAttr,
        &ioStatus,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ,
        FILE_OPEN,
        FILE_NON_DIRECTORY_FILE,
        NULL,
        0
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("VirtualCdrom: Failed to open ISO file: 0x%08X\n", status));
        return status;
    }
    
    status = ZwQueryInformationFile(
        DevExt->IsoFileHandle,
        &ioStatus,
        &fileInfo,
        sizeof(fileInfo),
        FileStandardInformation
    );
    
    if (!NT_SUCCESS(status)) {
        ZwClose(DevExt->IsoFileHandle);
        DevExt->IsoFileHandle = NULL;
        return status;
    }
    
    DevExt->IsoFileSize = fileInfo.EndOfFile;
    DevExt->TotalSectors = (ULONG)(DevExt->IsoFileSize.QuadPart / SECTOR_SIZE);
    
    KdPrint(("VirtualCdrom: ISO file opened, size: %I64d bytes, sectors: %lu\n", 
             DevExt->IsoFileSize.QuadPart, DevExt->TotalSectors));
    
    return STATUS_SUCCESS;
}

VOID
VcdCloseIsoFile(
    _In_ PVCD_DEVICE_EXTENSION DevExt
)
{
    PAGED_CODE();
    
    if (DevExt->IsoFileHandle != NULL) {
        ZwClose(DevExt->IsoFileHandle);
        DevExt->IsoFileHandle = NULL;
        DevExt->IsoFileObject = NULL;
    }
}

NTSTATUS
VcdReadIsoSectors(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ ULONG StartSector,
    _In_ ULONG SectorCount,
    _Out_ PVOID Buffer
)
{
    NTSTATUS status;
    IO_STATUS_BLOCK ioStatus;
    LARGE_INTEGER offset;
    ULONG length;
    
    PAGED_CODE();
    
    if (DevExt->IsoFileHandle == NULL) {
        return STATUS_NO_MEDIA_IN_DEVICE;
    }
    
    offset.QuadPart = (LONGLONG)StartSector * SECTOR_SIZE;
    length = SectorCount * SECTOR_SIZE;
    
    if (offset.QuadPart + length > DevExt->IsoFileSize.QuadPart) {
        if (offset.QuadPart >= DevExt->IsoFileSize.QuadPart) {
            return STATUS_INVALID_PARAMETER;
        }
        length = (ULONG)(DevExt->IsoFileSize.QuadPart - offset.QuadPart);
    }
    
    status = ZwReadFile(
        DevExt->IsoFileHandle,
        NULL,
        NULL,
        NULL,
        &ioStatus,
        Buffer,
        length,
        &offset,
        NULL
    );
    
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(DevExt->IsoFileHandle, Executive, KernelMode, FALSE, NULL);
        status = ioStatus.Status;
    }
    
    return status;
}

NTSTATUS
VcdInsertMedia(
    _In_ PVCD_DEVICE_EXTENSION DevExt
)
{
    NTSTATUS status;
    KIRQL oldIrql;
    
    PAGED_CODE();
    
    KeAcquireSpinLock(&DevExt->StateLock, &oldIrql);
    
    if (DevExt->State == VCD_STATE_MEDIA_INSERTED) {
        KeReleaseSpinLock(&DevExt->StateLock, oldIrql);
        return STATUS_SUCCESS;
    }
    
    KeReleaseSpinLock(&DevExt->StateLock, oldIrql);
    
    status = VcdOpenIsoFile(DevExt);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    KeAcquireSpinLock(&DevExt->StateLock, &oldIrql);
    DevExt->State = VCD_STATE_MEDIA_INSERTED;
    KeReleaseSpinLock(&DevExt->StateLock, oldIrql);
    
    KdPrint(("VirtualCdrom: Media inserted\n"));
    
    return STATUS_SUCCESS;
}

NTSTATUS
VcdEjectMedia(
    _In_ PVCD_DEVICE_EXTENSION DevExt
)
{
    KIRQL oldIrql;
    
    PAGED_CODE();
    
    KeAcquireSpinLock(&DevExt->StateLock, &oldIrql);
    
    if (DevExt->State == VCD_STATE_MEDIA_INSERTED) {
        VcdCloseIsoFile(DevExt);
        DevExt->State = VCD_STATE_MEDIA_EJECTED;
        KdPrint(("VirtualCdrom: Media ejected\n"));
    }
    
    KeReleaseSpinLock(&DevExt->StateLock, oldIrql);
    
    return STATUS_SUCCESS;
}

NTSTATUS
VcdHandleScsiCommand(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ PSCSI_PASS_THROUGH Srb,
    _In_ PIRP Irp
)
{
    NTSTATUS status;
    PVOID dataBuffer;
    ULONG dataBufferLength;
    PUCHAR cdb;
    
    PAGED_CODE();
    
    cdb = Srb->Cdb;
    
    if (Srb->DataBufferOffset != 0) {
        dataBuffer = (PUCHAR)Srb + Srb->DataBufferOffset;
        dataBufferLength = Srb->DataTransferLength;
    } else {
        dataBuffer = NULL;
        dataBufferLength = 0;
    }
    
    UCHAR operationCode = cdb[0];
    
    switch (operationCode) {
        case SCSIOP_INQUIRY:
        {
            if (dataBuffer == NULL || dataBufferLength < sizeof(INQUIRYDATA)) {
                return STATUS_BUFFER_TOO_SMALL;
            }
            status = VcdHandleInquiry(DevExt, Srb, (PINQUIRYDATA)dataBuffer, dataBufferLength);
            break;
        }
        
        case SCSIOP_READ_CAPACITY:
        {
            if (dataBuffer == NULL || dataBufferLength < sizeof(READ_CAPACITY_DATA)) {
                return STATUS_BUFFER_TOO_SMALL;
            }
            status = VcdHandleReadCapacity(DevExt, (PREAD_CAPACITY_DATA)dataBuffer, dataBufferLength);
            break;
        }
        
        case SCSIOP_READ_TOC:
        {
            if (DevExt->State != VCD_STATE_MEDIA_INSERTED) {
                return STATUS_NO_MEDIA_IN_DEVICE;
            }
            if (dataBuffer == NULL || dataBufferLength < sizeof(CDROM_TOC)) {
                return STATUS_BUFFER_TOO_SMALL;
            }
            status = VcdHandleReadToc(DevExt, (PCDROM_TOC)dataBuffer, dataBufferLength);
            break;
        }
        
        case SCSIOP_READ:
        case SCSIOP_READ12:
        case SCSIOP_READ16:
        {
            ULONG startSector;
            USHORT sectorCount;
            
            if (DevExt->State != VCD_STATE_MEDIA_INSERTED) {
                return STATUS_NO_MEDIA_IN_DEVICE;
            }
            
            if (operationCode == SCSIOP_READ) {
                startSector = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
                sectorCount = (cdb[7] << 8) | cdb[8];
            } else if (operationCode == SCSIOP_READ12) {
                startSector = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
                sectorCount = (USHORT)((cdb[6] << 24) | (cdb[7] << 16) | (cdb[8] << 8) | cdb[9]);
            } else {
                startSector = (ULONG)((cdb[2] << 56) | (cdb[3] << 48) | (cdb[4] << 40) | (cdb[5] << 32) |
                                      (cdb[6] << 24) | (cdb[7] << 16) | (cdb[8] << 8) | cdb[9]);
                sectorCount = (USHORT)((cdb->CDB16.TransferLength[0] << 24) |
                                       (cdb->CDB16.TransferLength[1] << 16) |
                                       (cdb->CDB16.TransferLength[2] << 8) |
                                       cdb->CDB16.TransferLength[3]);
            }
            
            status = VcdHandleReadSectors(DevExt, startSector, sectorCount, dataBuffer, dataBufferLength);
            break;
        }
        
        case SCSIOP_START_STOP_UNIT:
        {
            BOOLEAN start = (cdb[4] & 0x01) != 0;
            BOOLEAN loadEject = (cdb[4] & 0x02) != 0;
            status = VcdHandleStartStopUnit(DevExt, start, loadEject);
            break;
        }
        
        case SCSIOP_MODE_SENSE:
        case SCSIOP_MODE_SENSE10:
        {
            if (dataBuffer == NULL) {
                return STATUS_BUFFER_TOO_SMALL;
            }
            status = VcdHandleModeSense(DevExt, (PMODE_PARAMETER_HEADER)dataBuffer, dataBufferLength);
            break;
        }
        
        case SCSIOP_TEST_UNIT_READY:
        {
            if (DevExt->State != VCD_STATE_MEDIA_INSERTED) {
                status = STATUS_NO_MEDIA_IN_DEVICE;
            } else {
                status = STATUS_SUCCESS;
            }
            break;
        }
        
        case SCSIOP_PREVENT_ALLOW_MEDIUM_REMOVAL:
        {
            status = STATUS_SUCCESS;
            break;
        }
        
        default:
        {
            KdPrint(("VirtualCdrom: Unsupported SCSI command: 0x%02X\n", cdb->CDB6GENERIC.OperationCode));
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
    }
    
    Srb->ScsiStatus = SCSISTAT_GOOD;
    Irp->IoStatus.Information = Srb->DataTransferLength;
    
    return status;
}

NTSTATUS
VcdHandleInquiry(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ PSCSI_PASS_THROUGH Srb,
    _In_ PINQUIRYDATA InquiryBuffer,
    _In_ ULONG BufferLength
)
{
    PAGED_CODE();
    
    UNREFERENCED_PARAMETER(Srb);
    
    if (BufferLength > sizeof(INQUIRYDATA)) {
        BufferLength = sizeof(INQUIRYDATA);
    }
    
    RtlCopyMemory(InquiryBuffer, &DevExt->InquiryData, BufferLength);
    
    return STATUS_SUCCESS;
}

NTSTATUS
VcdHandleReadCapacity(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ PREAD_CAPACITY_DATA ReadCapacityBuffer,
    _In_ ULONG BufferLength
)
{
    PAGED_CODE();
    
    UNREFERENCED_PARAMETER(BufferLength);
    
    if (DevExt->State != VCD_STATE_MEDIA_INSERTED) {
        return STATUS_NO_MEDIA_IN_DEVICE;
    }
    
    ReadCapacityBuffer->LogicalBlockAddress = _byteswap_ulong(DevExt->TotalSectors - 1);
    ReadCapacityBuffer->BytesPerBlock = _byteswap_ulong(SECTOR_SIZE);
    
    return STATUS_SUCCESS;
}

NTSTATUS
VcdHandleReadToc(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ PCDROM_TOC TocBuffer,
    _In_ ULONG BufferLength
)
{
    PAGED_CODE();
    
    UNREFERENCED_PARAMETER(BufferLength);
    
    RtlCopyMemory(TocBuffer, &DevExt->CdromToc, sizeof(CDROM_TOC));
    
    return STATUS_SUCCESS;
}

NTSTATUS
VcdHandleReadSectors(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ ULONG StartSector,
    _In_ USHORT SectorCount,
    _Out_ PVOID Buffer,
    _In_ ULONG BufferLength
)
{
    PAGED_CODE();
    
    if ((ULONG)SectorCount * SECTOR_SIZE > BufferLength) {
        SectorCount = (USHORT)(BufferLength / SECTOR_SIZE);
    }
    
    return VcdReadIsoSectors(DevExt, StartSector, SectorCount, Buffer);
}

NTSTATUS
VcdHandleStartStopUnit(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ BOOLEAN Start,
    _In_ BOOLEAN LoadEject
)
{
    PAGED_CODE();
    
    if (LoadEject) {
        if (Start) {
            return VcdInsertMedia(DevExt);
        } else {
            return VcdEjectMedia(DevExt);
        }
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS
VcdHandleModeSense(
    _In_ PVCD_DEVICE_EXTENSION DevExt,
    _In_ PMODE_PARAMETER_HEADER ModeBuffer,
    _In_ ULONG BufferLength
)
{
    PMODE_CDROM_PAGE cdromPage;
    
    PAGED_CODE();
    
    UNREFERENCED_PARAMETER(DevExt);
    
    if (BufferLength < sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_CDROM_PAGE)) {
        return STATUS_BUFFER_TOO_SMALL;
    }
    
    RtlZeroMemory(ModeBuffer, BufferLength);
    
    ModeBuffer->ModeDataLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_CDROM_PAGE) - 1;
    ModeBuffer->MediumType = 0;
    ModeBuffer->DeviceSpecificParameter = 0;
    ModeBuffer->BlockDescriptorLength = 0;
    
    cdromPage = (PMODE_CDROM_PAGE)((PUCHAR)ModeBuffer + sizeof(MODE_PARAMETER_HEADER));
    cdromPage->PageCode = MODE_PAGE_ERROR_RECOVERY;
    cdromPage->PageLength = sizeof(MODE_CDROM_PAGE) - 2;
    cdromPage->ErrorRecoveryFlags = 0;
    cdromPage->ReadRetryCount = 0;
    cdromPage->CorrectionSpan = 0;
    cdromPage->HeadOffsetCount = 0;
    cdromPage->DataStrobeOffsetCount = 0;
    cdromPage->Reserved2 = 0;
    cdromPage->WriteRetryCount = 0;
    cdromPage->Reserved3 = 0;
    cdromPage->RecoveryTimeLimit = 0;
    
    return STATUS_SUCCESS;
}

VOID
VcdInitializeInquiryData(
    _In_ PVCD_DEVICE_EXTENSION DevExt
)
{
    PINQUIRYDATA inquiry;
    
    PAGED_CODE();
    
    inquiry = &DevExt->InquiryData;
    RtlZeroMemory(inquiry, sizeof(INQUIRYDATA));
    
    inquiry->DeviceType = READ_ONLY_DIRECT_ACCESS_DEVICE;
    inquiry->DeviceTypeQualifier = DEVICE_CONNECTED;
    inquiry->RemovableMedia = 1;
    inquiry->CommandQueue = 0;
    inquiry->SoftReset = 0;
    inquiry->RelativeAddressing = 0;
    inquiry->Wide32Bit = 0;
    inquiry->Wide16Bit = 0;
    inquiry->Synchronous = 0;
    inquiry->LinkedCommands = 0;
    inquiry->Reserved = 0;
    inquiry->VendorUnique = 0;
    inquiry->AdditionalLength = sizeof(INQUIRYDATA) - 5;
    
    RtlCopyMemory(inquiry->VendorId, "Virtual ", 8);
    RtlCopyMemory(inquiry->ProductId, "DVD-ROM         ", 16);
    RtlCopyMemory(inquiry->ProductRevisionLevel, "1.00", 4);
}

VOID
VcdInitializeToc(
    _In_ PVCD_DEVICE_EXTENSION DevExt
)
{
    PCDROM_TOC toc;
    
    PAGED_CODE();
    
    toc = &DevExt->CdromToc;
    RtlZeroMemory(toc, sizeof(CDROM_TOC));
    
    toc->Length[0] = 0x00;
    toc->Length[1] = 0x1A;
    
    toc->TrackData[0].Reserved = 0;
    toc->TrackData[0].Control = TOC_DATA_TRACK | TOC_CONTROL_DATA_TRACK;
    toc->TrackData[0].TrackNumber = 1;
    toc->TrackData[0].Reserved1 = 0;
    toc->TrackData[0].Address[0] = 0;
    toc->TrackData[0].Address[1] = 0;
    toc->TrackData[0].Address[2] = 2;
    toc->TrackData[0].Address[3] = 0;
    
    toc->TrackData[1].Reserved = 0;
    toc->TrackData[1].Control = TOC_DATA_TRACK | TOC_CONTROL_DATA_TRACK;
    toc->TrackData[1].TrackNumber = 0xAA;
    toc->TrackData[1].Reserved1 = 0;
    toc->TrackData[1].Address[0] = 0;
    toc->TrackData[1].Address[1] = 0;
    toc->TrackData[1].Address[2] = 0;
    toc->TrackData[1].Address[3] = 0;
}
