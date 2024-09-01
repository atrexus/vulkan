#pragma once

#include <windows.h>
#include <winternl.h>

//
// Define the maximum number of syscall entries. This value is arbitrary and can be changed.
//
#define MAX_SYSCALLS 600

//
// Define a macro to convert a relative virtual address (RVA) to a virtual address (VA).
//
#define RVA2VA(Type, DllBase, Rva) (Type)((ULONG_PTR)DllBase + Rva)

//
// Represents a single syscall entry.
//
typedef struct _SYSCALL_ENTRY
{
    DWORD Hash;           // Hash of the syscall name.
    DWORD Rva;            // Relative virtual address of the syscall stub.
    PVOID SyscallAddress; // Address of the syscall stub.
    PCHAR Name;           // Name of the syscall.
} SYSCALL_ENTRY, *PSYSCALL_ENTRY;

//
// Represents a list of syscall entries.
//
typedef struct _SYSCALL_LIST
{
    DWORD Entries;                     // Number of syscall entries.
    SYSCALL_ENTRY Table[MAX_SYSCALLS]; // Array of syscall entries.
} SYSCALL_LIST, *PSYSCALL_LIST;

_Success_(return)
BOOL
GetSyscallList(_Out_ PSYSCALL_LIST List);

_Success_(return >= 0)
EXTERN_C
DWORD
GetSyscallNumber(_In_ PSYSCALL_LIST List, _In_ DWORD FunctionHash);

_Success_(return != NULL)
PVOID
GetSyscallAddress(_In_ PSYSCALL_LIST List, _In_ DWORD FunctionHash);

_Success_(return >= 0)
DWORD
GetSyscallHash(_In_ PSYSCALL_LIST List, _In_ PCHAR FunctionName);