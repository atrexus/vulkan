#include "nt.h"
#include "syscalls.h"

static SYSCALL_LIST NtSyscallList;

_Success_(return)
BOOL
NtInitialize()
{
    return GetSyscallList(&NtSyscallList);
}

_Success_(return >= 0)
EXTERN_C
DWORD
NtGetSSN(DWORD FunctionHash)
{
    return GetSyscallNumber(&NtSyscallList, FunctionHash);
}