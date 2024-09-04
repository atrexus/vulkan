#include "sections.h"
#include "out.h"
#include "syscalls.h"
#include "imports.h"

_Success_(return > 0)
static DWORD
GetProcessIdByName(_In_ LPCWSTR ProcessName);

_Success_(return)
static BOOL
GetModuleInfo(_In_ HANDLE Process, _In_ LPWSTR ModuleName, _Out_ MODULEINFO *ModuleInfo);

_Success_(return)
static BOOL
WriteImageToDisk(_In_ PDUMPER Dumper, _In_ PBYTE Buffer, _In_ SIZE_T Size);

_Success_(return)
static BOOL
BuildInitialImage(_In_ PDUMPER Dumper, _Out_ PBYTE *Buffer);

// =====================================================================================================================
// Public functions
// =====================================================================================================================

_Success_(return)
BOOL
DumperCreate(_Out_ PDUMPER Dumper, _In_ LPWSTR ProcessName, _In_ LPWSTR OutputPath, _In_ FLOAT DecryptionFactor)
{
    OBJECT_ATTRIBUTES ObjectAttributes;
    CLIENT_ID ClientId;
    NTSTATUS Status;
    HMODULE Module;

    Dumper->ProcessName = ProcessName;
    Dumper->OutputPath = OutputPath;
    Dumper->DecryptionFactor = DecryptionFactor;
    Dumper->ProcessId = 0;

    //
    // Initialize the syscall list.
    //
    if (!NtInitialize())
    {
        error("Failed to initialize the syscall list");
        return FALSE;
    }

    //
    // Get the process ID of the target process.
    //
    Dumper->ProcessId = GetProcessIdByName(ProcessName);

    if (!Dumper->ProcessId)
    {
        error("Failed to get the process ID of the target process");
        return FALSE;
    }

    InitializeObjectAttributes(&ObjectAttributes, NULL, 0, NULL, NULL);

    ClientId.UniqueProcess = Dumper->ProcessId;
    ClientId.UniqueThread = 0;

    //
    // Open a handle to the target process.
    //
    Status = NtOpenProcess(&Dumper->Process, PROCESS_ALL_ACCESS, &ObjectAttributes, &ClientId);

    if (!NT_SUCCESS(Status))
    {
        error("Failed to open a handle to the target process (0x%08X)", Status);
        return FALSE;
    }

    //
    // Get the module information of the target process.
    //
    if (!GetModuleInfo(Dumper->Process, ProcessName, &Dumper->ModuleInfo))
    {
        error("Failed to get the module information of the target process");
        return FALSE;
    }

    return TRUE;
}

_Success_(return)
BOOL
DumperDumpToDisk(_In_ PDUMPER Dumper)
{
    PBYTE Buffer;
    BOOL Success;

    Success = FALSE;

    //
    // Read the entire image of the target process.
    //
    if (!BuildInitialImage(Dumper, &Buffer))
    {
        goto Exit;
    }

    //
    // Resolve the sections of the image.
    //
    if (!ResolveSections(Dumper, &Buffer))
    {
        error("Failed to resolve the sections of the image");
        goto Exit;
    }

    //
    // Resolve the imports of the image.
    //
    if (!ResolveImports(Dumper, &Buffer))
    {
        error("Failed to resolve the imports of the image");
        goto Exit;
    }

    //
    // Now we can write the image to disk.
    //
    if (!WriteImageToDisk(Dumper, Buffer, Dumper->ModuleInfo.SizeOfImage))
    {
        error("Failed to write the image to disk");
        goto Exit;
    }

    Success = TRUE;

Exit:
    free(Buffer);

    return Success;
}

_Success_(return == EXIT_SUCCESS)
INT
DumperDestroy(_In_ PDUMPER Dumper)
{
    NTSTATUS Status;

    //
    // Close the handle to the target process.
    //
    Status = NtClose(Dumper->Process);

    return !NT_SUCCESS(Status);
}

// =====================================================================================================================
// Private functions
// =====================================================================================================================

_Success_(return > 0)
DWORD
GetProcessIdByName(_In_ LPCWSTR ProcessName)
{
    NTSTATUS Status;
    PSYSTEM_PROCESS_INFORMATION ProcessInfo;
    PVOID Buffer, TempBuffer;
    ULONG BufferSize;
    DWORD ProcessId;

    BufferSize = NtBufferSize;
    Buffer = NULL;
    TempBuffer = NULL;
    ProcessId = 0;

    //
    // Reallocate the buffer until it's large enough to store the process information.
    //
    do
    {
        TempBuffer = realloc(Buffer, BufferSize);

        if (TempBuffer == NULL)
        {
            free(Buffer);
            return 0;
        }

        Buffer = TempBuffer;
    } while ((Status = NtQuerySystemInformation(SystemProcessInformation, Buffer, BufferSize, &BufferSize)) ==
             STATUS_INFO_LENGTH_MISMATCH);

    //
    // Check if the system call was successful.
    //
    if (NT_SUCCESS(Status))
    {
        //
        // Iterate over the process list.
        //
        for (ProcessInfo = (PSYSTEM_PROCESS_INFORMATION)Buffer; ProcessInfo->NextEntryOffset != 0;
             ProcessInfo = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)ProcessInfo + ProcessInfo->NextEntryOffset))
        {
            //
            // Check if the process name matches the target process name.
            //
            if (ProcessInfo->ImageName.Buffer && !_wcsicmp(ProcessInfo->ImageName.Buffer, ProcessName))
            {
                ProcessId = (DWORD)ProcessInfo->UniqueProcessId;
                break;
            }
        }
    }

    free(Buffer);
    return ProcessId;
}

_Success_(return)
static BOOL
GetModuleInfo(_In_ HANDLE Process, _In_ LPWSTR ModuleName, _Out_ MODULEINFO *ModuleInfo)
{
    HMODULE Modules[1024];
    DWORD Needed;
    DWORD i;

    if (!EnumProcessModules(Process, Modules, sizeof(Modules), &Needed))
    {
        return FALSE;
    }

    for (i = 0; i < Needed / sizeof(HMODULE); i++)
    {
        WCHAR Name[MAX_PATH];

        if (!GetModuleFileNameExW(Process, Modules[i], Name, MAX_PATH))
        {
            continue;
        }

        if (!_wcsicmp(wcsrchr(Name, L'\\') + 1, ModuleName))
        {
            return GetModuleInformation(Process, Modules[i], ModuleInfo, sizeof(MODULEINFO));
        }
    }
    return FALSE;
}

_Success_(return)
static BOOL
WriteImageToDisk(_In_ PDUMPER Dumper, _In_ PBYTE Buffer, _In_ SIZE_T Size)
{
    WCHAR Path[MAX_PATH];
    HANDLE File;
    DWORD BytesWritten;

    //
    // Construct the path to the output file.
    //
    swprintf(Path, MAX_PATH, L"%s\\%s", Dumper->OutputPath, Dumper->ProcessName);

    //
    // Open the file for writing.
    //
    File = CreateFileW(Path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (File == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    //
    // Write the image to the file.
    //
    if (!WriteFile(File, Buffer, Size, &BytesWritten, NULL))
    {
        CloseHandle(File);
        return FALSE;
    }

    info("Successfully dumped image to disk (path: %ws)", Path);

    //
    // Close the file handle.
    //
    CloseHandle(File);

    return TRUE;
}

_Success_(return)
static BOOL
BuildInitialImage(_In_ PDUMPER Dumper, _Out_ PBYTE *Buffer)
{
    PVOID BaseAddress;
    MEMORY_BASIC_INFORMATION MemoryInfo;
    NTSTATUS Status;

    //
    // Allocate a buffer to store the image.
    //
    *Buffer = malloc(Dumper->ModuleInfo.SizeOfImage);

    if (*Buffer == NULL)
    {
        error("Failed to allocate memory for the image buffer");
        return FALSE;
    }

    //
    // Fill the buffer with zeros.
    //
    ZeroMemory(*Buffer, Dumper->ModuleInfo.SizeOfImage);

    //
    // The initial image will only contain the PE headers. This is the first memory region of the target process.
    //
    BaseAddress = Dumper->ModuleInfo.lpBaseOfDll;

    //
    // Query the first memory region of the target process.
    //
    if (!VirtualQueryEx(Dumper->Process, BaseAddress, &MemoryInfo, sizeof(MemoryInfo)))
    {
        error("Failed to query memory region at 0x%p", BaseAddress);
        return FALSE;
    }

    //
    // Read the first memory region of the target process and store it in the buffer.
    //
    Status = NtReadVirtualMemory(Dumper->Process, BaseAddress, *Buffer, MemoryInfo.RegionSize, NULL);

    if (!NT_SUCCESS(Status))
    {
        error("Failed to read memory region at 0x%p (0x%08X)", BaseAddress, Status);
        return FALSE;
    }

    info("Built initial image of target process (0x%p)", BaseAddress);
    return TRUE;
}