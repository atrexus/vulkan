#include "syscalls.h"

#define SEED 0x95DF09BA
#define ROL8(v) (v << 8 | v >> 24)
#define ROR8(v) (v >> 8 | v << 24)
#define ROX8(v) ((SEED % 2) ? ROL8(v) : ROR8(v))

static DWORD
HashSyscall(PCSTR FunctionName)
{
    DWORD Hash = SEED;
    DWORD i = 0;

    while (FunctionName[i] != 0)
        Hash ^= *(WORD *)((ULONG_PTR)FunctionName + i++) + ROR8(Hash);

    return Hash;
}

_Success_(return)
BOOL
GetSyscallList(_Out_ PSYSCALL_LIST List)
{
    PPEB_LDR_DATA Ldr;
    PLDR_DATA_TABLE_ENTRY LdrEntry;
    PIMAGE_DOS_HEADER DosHeader;
    PIMAGE_NT_HEADERS NtHeaders;
    DWORD i, j, NumberOfNames, VirtualAddress, Entries = 0;
    PIMAGE_DATA_DIRECTORY DataDirectory;
    PIMAGE_EXPORT_DIRECTORY ExportDirectory;
    PDWORD Functions;
    PDWORD Names;
    PWORD Ordinals;
    PCHAR DllName, FunctionName;
    PVOID DllBase;
    PSYSCALL_ENTRY Table;
    SYSCALL_ENTRY Entry;

    //
    // Get the PEB.
    //
    Ldr = ((PPEB)__readgsqword(0x60))->Ldr;
    ExportDirectory = NULL;
    DllBase = NULL;

    //
    // Get the base address of ntdll.dll. It's not guaranteed that ntdll.dll is the second module in the list, so we
    // will loop through the list until we find it.
    //
    for (LdrEntry = (PLDR_DATA_TABLE_ENTRY)Ldr->Reserved2[1]; LdrEntry->DllBase != NULL;
         LdrEntry = (PLDR_DATA_TABLE_ENTRY)LdrEntry->Reserved1[0])
    {
        DllBase = LdrEntry->DllBase;
        DosHeader = (PIMAGE_DOS_HEADER)DllBase;
        NtHeaders = RVA2VA(PIMAGE_NT_HEADERS, DllBase, DosHeader->e_lfanew);
        DataDirectory = (PIMAGE_DATA_DIRECTORY)NtHeaders->OptionalHeader.DataDirectory;
        VirtualAddress = DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

        //
        // If the virtual address of the export directory is zero, then the module doesn't export any functions.
        //
        if (!VirtualAddress)
            continue;

        ExportDirectory = RVA2VA(PIMAGE_EXPORT_DIRECTORY, DllBase, VirtualAddress);

        //
        // If this is the target DLL, then break out of the loop.
        //
        DllName = RVA2VA(PCHAR, DllBase, ExportDirectory->Name);

        if ((*(ULONG *)DllName | 0x20202020) != 0x6c64746e)
            continue;
        if ((*(ULONG *)(DllName + 4) | 0x20202020) == 0x6c642e6c)
            break;
    }

    //
    // If the export directory is null, then the module doesn't export any functions.
    //
    if (!ExportDirectory)
        return FALSE;

    NumberOfNames = ExportDirectory->NumberOfNames;

    Functions = RVA2VA(PDWORD, DllBase, ExportDirectory->AddressOfFunctions);
    Names = RVA2VA(PDWORD, DllBase, ExportDirectory->AddressOfNames);
    Ordinals = RVA2VA(PWORD, DllBase, ExportDirectory->AddressOfNameOrdinals);

    Table = List->Table;

    do
    {
        FunctionName = RVA2VA(PCHAR, DllBase, Names[NumberOfNames - 1]);
        //
        // Is this a system call?
        //
        if (*(USHORT *)FunctionName == 0x775a)
        {
            //
            // Save Hash of system call and the address.
            //
            Table[Entries].Hash = HashSyscall(FunctionName);
            Table[Entries].Rva = Functions[Ordinals[NumberOfNames - 1]];
            Table[Entries].SyscallAddress = RVA2VA(PVOID, DllBase, Table[Entries].Rva);
            Table[Entries].Name = FunctionName;

            if (++Entries == MAX_SYSCALLS)
                break;
        }
    } while (--NumberOfNames);

    //
    // Save the number of system calls.
    //
    List->Entries = Entries;

    //
    // Sort the list by address in ascending order.
    //
    for (i = 0; i < Entries - 1; i++)
    {
        for (j = 0; j < Entries - i - 1; j++)
        {
            if (Table[j].Rva > Table[j + 1].Rva)
            {
                //
                // Swap entries.
                //
                Entry.Hash = Table[j].Hash;
                Entry.Rva = Table[j].Rva;
                Entry.Name = Table[j].Name;
                Entry.SyscallAddress = Table[j].SyscallAddress;

                Table[j].Hash = Table[j + 1].Hash;
                Table[j].Rva = Table[j + 1].Rva;
                Table[j].Name = Table[j + 1].Name;
                Table[j].SyscallAddress = Table[j + 1].SyscallAddress;

                Table[j + 1].Hash = Entry.Hash;
                Table[j + 1].Rva = Entry.Rva;
                Table[j + 1].Name = Entry.Name;
                Table[j + 1].SyscallAddress = Entry.SyscallAddress;
            }
        }
    }

    return TRUE;
}

_Success_(return >= 0)
EXTERN_C
DWORD
GetSyscallNumber(_In_ PSYSCALL_LIST List, _In_ DWORD FunctionHash)
{
    DWORD i;

    for (i = 0; i < List->Entries; i++)
    {
        if (List->Table[i].Hash == FunctionHash)
            return i;
    }

    return -1;
}

_Success_(return)
PVOID
GetSyscallAddress(_In_ PSYSCALL_LIST List, _In_ DWORD FunctionHash)
{
    DWORD i;

    for (i = 0; i < List->Entries; i++)
    {
        if (List->Table[i].Hash == FunctionHash)
            return List->Table[i].SyscallAddress;
    }

    return -1;
}

_Success_(return >= 0)
DWORD
GetSyscallHash(_In_ PSYSCALL_LIST List, _In_ PCHAR FunctionName)
{
    DWORD i;

    for (i = 0; i < List->Entries; i++)
    {
        if (strcmp(List->Table[i].Name, FunctionName) == 0)
            return List->Table[i].Hash;
    }

    return -1;
}