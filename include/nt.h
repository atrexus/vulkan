//
// nt.h - This file contains the definitions for the NT kernel functions.
//

#pragma once

#include <windows.h>
#include <winternl.h>

//
// NT Error codes.
//
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)

//
// The size of the buffer used to store the NT image.
//
#define NtBufferSize 0x10000

//
// Standard size of a page in memory.
//
#define PAGE_SIZE 0x1000

typedef CLIENT_ID *PCLIENT_ID;

typedef struct _PROCESS_HANDLE_TABLE_ENTRY_INFO
{
    HANDLE HandleValue;
    ULONG_PTR HandleCount;
    ULONG_PTR PointerCount;
    ACCESS_MASK GrantedAccess;
    ULONG ObjectTypeIndex;
    ULONG HandleAttributes;
    ULONG Reserved;
} PROCESS_HANDLE_TABLE_ENTRY_INFO, *PPROCESS_HANDLE_TABLE_ENTRY_INFO;

typedef struct _PROCESS_HANDLE_SNAPSHOT_INFORMATION
{
    ULONG_PTR NumberOfHandles;
    ULONG_PTR Reserved;
    PROCESS_HANDLE_TABLE_ENTRY_INFO Handles[ANYSIZE_ARRAY];
} PROCESS_HANDLE_SNAPSHOT_INFORMATION, *PPROCESS_HANDLE_SNAPSHOT_INFORMATION;

typedef struct _TP_TASK_CALLBACKS
{
    void *ExecuteCallback;
    void *Unposted;
} TP_TASK_CALLBACKS, *PTP_TASK_CALLBACKS;

typedef struct _TP_TASK
{
    PTP_TASK_CALLBACKS Callbacks;
    UINT32 NumaNode;
    UINT8 IdealProcessor;
    char Padding_242[3];
    LIST_ENTRY ListEntry;
} TP_TASK, *PTP_TASK;

typedef struct _TP_DIRECT
{
    TP_TASK Task;
    UINT64 Lock;
    LIST_ENTRY IoCompletionInformationList;
    void *Callback;
    UINT32 NumaNode;
    UINT8 IdealProcessor;
    char __PADDING__[3];
} TP_DIRECT, *PTP_DIRECT;

//
// Initializes the NT kernel functions.
//
_Success_(return)
BOOL
NtInitialize();

//
// Gets the system service number (SSN) for a given NT function hash.
//
_Success_(return >= 0)
EXTERN_C
DWORD
NtGetSSN(DWORD FunctionHash);

// =====================================================================================================================
// Begin NT kernel functions. Each of these must have a definition in ASM
// =====================================================================================================================

EXTERN_C
NTSTATUS
NtOpenProcess(
    _Out_ PHANDLE ProcessHandle,
    _In_ ACCESS_MASK AccessMask,
    _In_ POBJECT_ATTRIBUTES ObjectAttributes,
    _In_ PCLIENT_ID ClientId);

EXTERN_C
NTSTATUS
NtAllocateVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _Inout_ PVOID *BaseAddress,
    _In_ ULONG ZeroBits,
    _Inout_ PSIZE_T RegionSize,
    _In_ ULONG AllocationType,
    _In_ ULONG Protect);

EXTERN_C
NTSTATUS
NtWriteVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_ PVOID BaseAddress,
    _In_ PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T BytesWritten);

EXTERN_C
NTSTATUS
NtSetIoCompletion(
    _In_ HANDLE IoCompletionHandle,
    _In_ PVOID KeyContext,
    _In_opt_ PVOID ApcContext,
    _In_opt_ NTSTATUS IoStatus,
    _In_opt_ ULONG_PTR IoStatusInformatio);

EXTERN_C
NTSTATUS
NtReadVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_ PVOID BaseAddress,
    _Out_ PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T BytesRead);

EXTERN_C
NTSTATUS
NtSuspendProcess(_In_ HANDLE Handle);

EXTERN_C
NTSTATUS
NtResumeProcess(_In_ HANDLE Handle);