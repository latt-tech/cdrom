#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VCD_DEVICE_NAME "\\\\.\\VirtualCdrom"
#define VCD_SERVICE_NAME "VirtualCdrom"
#define VCD_DRIVER_PATH "C:\\Windows\\System32\\drivers\\VirtualCdrom.sys"

void PrintUsage(const char* progName) {
    printf("Usage: %s <command>\n", progName);
    printf("Commands:\n");
    printf("  install    - Install the virtual CDROM driver\n");
    printf("  uninstall  - Uninstall the virtual CDROM driver\n");
    printf("  start      - Start the virtual CDROM service\n");
    printf("  stop       - Stop the virtual CDROM service\n");
    printf("  status     - Check driver status\n");
    printf("  insert     - Insert media (load ISO)\n");
    printf("  eject      - Eject media\n");
}

BOOL InstallDriver() {
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    BOOL result = FALSE;
    char driverPath[MAX_PATH];
    
    if (!GetFullPathNameA("VirtualCdrom.sys", MAX_PATH, driverPath, NULL)) {
        printf("Failed to get driver path: %lu\n", GetLastError());
        return FALSE;
    }
    
    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCManager == NULL) {
        printf("Failed to open Service Control Manager: %lu\n", GetLastError());
        return FALSE;
    }
    
    hService = CreateServiceA(
        hSCManager,
        VCD_SERVICE_NAME,
        "Virtual CDROM Driver",
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        driverPath,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    );
    
    if (hService == NULL) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_EXISTS) {
            printf("Driver is already installed.\n");
            hService = OpenServiceA(hSCManager, VCD_SERVICE_NAME, SERVICE_ALL_ACCESS);
            if (hService == NULL) {
                printf("Failed to open service: %lu\n", GetLastError());
                CloseServiceHandle(hSCManager);
                return FALSE;
            }
        } else {
            printf("Failed to create service: %lu\n", error);
            CloseServiceHandle(hSCManager);
            return FALSE;
        }
    } else {
        printf("Driver installed successfully.\n");
    }
    
    result = TRUE;
    
    if (hService) CloseServiceHandle(hService);
    if (hSCManager) CloseServiceHandle(hSCManager);
    
    return result;
}

BOOL UninstallDriver() {
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    BOOL result = FALSE;
    
    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCManager == NULL) {
        printf("Failed to open Service Control Manager: %lu\n", GetLastError());
        return FALSE;
    }
    
    hService = OpenServiceA(hSCManager, VCD_SERVICE_NAME, SERVICE_ALL_ACCESS);
    if (hService == NULL) {
        printf("Failed to open service: %lu\n", GetLastError());
        CloseServiceHandle(hSCManager);
        return FALSE;
    }
    
    SERVICE_STATUS serviceStatus;
    ControlService(hService, SERVICE_CONTROL_STOP, &serviceStatus);
    
    if (DeleteService(hService)) {
        printf("Driver uninstalled successfully.\n");
        result = TRUE;
    } else {
        printf("Failed to delete service: %lu\n", GetLastError());
    }
    
    if (hService) CloseServiceHandle(hService);
    if (hSCManager) CloseServiceHandle(hSCManager);
    
    return result;
}

BOOL StartDriver() {
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    BOOL result = FALSE;
    
    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCManager == NULL) {
        printf("Failed to open Service Control Manager: %lu\n", GetLastError());
        return FALSE;
    }
    
    hService = OpenServiceA(hSCManager, VCD_SERVICE_NAME, SERVICE_ALL_ACCESS);
    if (hService == NULL) {
        printf("Failed to open service: %lu\n", GetLastError());
        CloseServiceHandle(hSCManager);
        return FALSE;
    }
    
    if (StartServiceA(hService, 0, NULL)) {
        printf("Driver started successfully.\n");
        result = TRUE;
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_ALREADY_RUNNING) {
            printf("Driver is already running.\n");
            result = TRUE;
        } else {
            printf("Failed to start driver: %lu\n", error);
        }
    }
    
    if (hService) CloseServiceHandle(hService);
    if (hSCManager) CloseServiceHandle(hSCManager);
    
    return result;
}

BOOL StopDriver() {
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    BOOL result = FALSE;
    SERVICE_STATUS serviceStatus;
    
    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCManager == NULL) {
        printf("Failed to open Service Control Manager: %lu\n", GetLastError());
        return FALSE;
    }
    
    hService = OpenServiceA(hSCManager, VCD_SERVICE_NAME, SERVICE_ALL_ACCESS);
    if (hService == NULL) {
        printf("Failed to open service: %lu\n", GetLastError());
        CloseServiceHandle(hSCManager);
        return FALSE;
    }
    
    if (ControlService(hService, SERVICE_CONTROL_STOP, &serviceStatus)) {
        printf("Driver stopped successfully.\n");
        result = TRUE;
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_NOT_ACTIVE) {
            printf("Driver is not running.\n");
            result = TRUE;
        } else {
            printf("Failed to stop driver: %lu\n", error);
        }
    }
    
    if (hService) CloseServiceHandle(hService);
    if (hSCManager) CloseServiceHandle(hSCManager);
    
    return result;
}

BOOL CheckStatus() {
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    BOOL result = FALSE;
    SERVICE_STATUS_PROCESS ssp;
    DWORD bytesNeeded;
    
    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (hSCManager == NULL) {
        printf("Failed to open Service Control Manager: %lu\n", GetLastError());
        return FALSE;
    }
    
    hService = OpenServiceA(hSCManager, VCD_SERVICE_NAME, SERVICE_QUERY_STATUS);
    if (hService == NULL) {
        printf("Driver is not installed.\n");
        CloseServiceHandle(hSCManager);
        return FALSE;
    }
    
    if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytesNeeded)) {
        printf("Driver Status: ");
        switch (ssp.dwCurrentState) {
            case SERVICE_RUNNING:
                printf("Running\n");
                break;
            case SERVICE_STOPPED:
                printf("Stopped\n");
                break;
            case SERVICE_START_PENDING:
                printf("Starting...\n");
                break;
            case SERVICE_STOP_PENDING:
                printf("Stopping...\n");
                break;
            default:
                printf("Unknown (%lu)\n", ssp.dwCurrentState);
                break;
        }
        result = TRUE;
    } else {
        printf("Failed to query service status: %lu\n", GetLastError());
    }
    
    if (hService) CloseServiceHandle(hService);
    if (hSCManager) CloseServiceHandle(hSCManager);
    
    return result;
}

BOOL InsertMedia() {
    HANDLE hDevice = CreateFileA(
        VCD_DEVICE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("Failed to open device: %lu\n", GetLastError());
        return FALSE;
    }
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_STORAGE_LOAD_MEDIA,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    if (result) {
        printf("Media inserted successfully.\n");
    } else {
        printf("Failed to insert media: %lu\n", GetLastError());
    }
    
    CloseHandle(hDevice);
    return result;
}

BOOL EjectMedia() {
    HANDLE hDevice = CreateFileA(
        VCD_DEVICE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("Failed to open device: %lu\n", GetLastError());
        return FALSE;
    }
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_STORAGE_EJECT_MEDIA,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    if (result) {
        printf("Media ejected successfully.\n");
    } else {
        printf("Failed to eject media: %lu\n", GetLastError());
    }
    
    CloseHandle(hDevice);
    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }
    
    const char* command = argv[1];
    BOOL result = FALSE;
    
    if (_stricmp(command, "install") == 0) {
        result = InstallDriver();
    } else if (_stricmp(command, "uninstall") == 0) {
        result = UninstallDriver();
    } else if (_stricmp(command, "start") == 0) {
        result = StartDriver();
    } else if (_stricmp(command, "stop") == 0) {
        result = StopDriver();
    } else if (_stricmp(command, "status") == 0) {
        result = CheckStatus();
    } else if (_stricmp(command, "insert") == 0) {
        result = InsertMedia();
    } else if (_stricmp(command, "eject") == 0) {
        result = EjectMedia();
    } else {
        printf("Unknown command: %s\n", command);
        PrintUsage(argv[0]);
        return 1;
    }
    
    return result ? 0 : 1;
}
