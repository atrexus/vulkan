#include "imports.h"
#include "syscalls.h"
#include "out.h"

#include <tlhelp32.h>
#include <winnt.h>

typedef struct _MODULE_ENTRY
{
    MODULEENTRY32 ModuleEntry;
    LIST_ENTRY ListEntry;
} MODULE_ENTRY, *PMODULE_ENTRY;

static _Success_(return != NULL)
PMODULE_ENTRY
CreateModuleEntry(_In_ PMODULEENTRY32 ModuleEntry);

static VOID
FreeModuleEntryList(_In_ PLIST_ENTRY ListHead);

static void
InitializeListHead(LIST_ENTRY *listHead);

static void
InsertTailList(LIST_ENTRY *listHead, LIST_ENTRY *newNode);

static int
IsListEmpty(LIST_ENTRY *listHead);

static void
RemoveEntryList(LIST_ENTRY *entry);

static _Success_(return != NULL)
PLIST_ENTRY
GetLoadedModules(_In_ DWORD ProcessId);


// =====================================================================================================================
// Public functions
// =====================================================================================================================

_Success_(return)
BOOL
ResolveImports(_In_ PDUMPER Dumper, _Inout_ PBYTE *OriginalImage)
{
    PLIST_ENTRY ModuleEntry;

    //
    // Get the loaded modules of the target process.
    //
    ModuleEntry = GetLoadedModules(Dumper->ProcessId);

    if (!ModuleEntry)
    {
        error("Failed to get the loaded modules of the target process");
        return FALSE;
    }

    return TRUE;
}

// =====================================================================================================================
// Private functions
// =====================================================================================================================
_Success_(return != NULL)
PMODULE_ENTRY
CreateModuleEntry(PMODULEENTRY32 ModuleEntry)
{
    PMODULE_ENTRY Entry;

    Entry = (PMODULE_ENTRY)malloc(sizeof(MODULE_ENTRY));

    if (Entry == NULL)
    {
        error("Failed to allocate memory for the module entry");
        return NULL;
    }

    Entry->ModuleEntry = *ModuleEntry;
    InitializeListHead(&Entry->ListEntry);

    return Entry;
}

VOID
FreeModuleEntryList(PLIST_ENTRY ListHead)
{
    PLIST_ENTRY entry = ListHead->Flink;
    while (entry != ListHead)
    {
        PMODULE_ENTRY node = CONTAINING_RECORD(entry, MODULE_ENTRY, ListEntry);
        entry = entry->Flink;
        free(node);
    }
}

_Success_(return != NULL)
PLIST_ENTRY
GetLoadedModules(DWORD ProcessId)
{
    HANDLE hSnapshot;
    MODULEENTRY32 ModuleEntry;
    PLIST_ENTRY ListHead;
    PMODULE_ENTRY Entry;

    //
    // Create a new module list head.
    //
    ListHead = (PLIST_ENTRY)malloc(sizeof(LIST_ENTRY));

    if (ListHead == NULL)
    {
        error("Failed to allocate memory for the module list head");
        return NULL;
    }

    //
    // Initialize the list head.
    //
    InitializeListHead(ListHead);

    //
    // Take a snapshot of all modules in the target process.
    //
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, ProcessId);

    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        error("Failed to create a snapshot of the target process modules");
        free(ListHead);
        return NULL;
    }

    //
    // Initialize the module entry structure.
    //
    ModuleEntry.dwSize = sizeof(MODULEENTRY32);

    //
    // Iterate over the modules in the target process.
    //
    if (Module32First(hSnapshot, &ModuleEntry))
    {
        do
        {
            if (Entry = CreateModuleEntry(&ModuleEntry))
            {
                InsertTailList(ListHead, &Entry->ListEntry);
            }
        } while (Module32Next(hSnapshot, &ModuleEntry));
    }

    //
    // Close the snapshot handle.
    //
    CloseHandle(hSnapshot);
    return ListHead;
}

void
InitializeListHead(LIST_ENTRY *listHead)
{
    listHead->Flink = listHead;
    listHead->Blink = listHead;
}

void
InsertTailList(LIST_ENTRY *listHead, LIST_ENTRY *newNode)
{
    LIST_ENTRY *last = listHead->Blink;

    newNode->Flink = listHead;
    newNode->Blink = last;

    last->Flink = newNode;
    listHead->Blink = newNode;
}

int
IsListEmpty(LIST_ENTRY *listHead)
{
    return listHead->Flink == listHead;
}

void
RemoveEntryList(LIST_ENTRY *entry)
{
    entry->Blink->Flink = entry->Flink;
    entry->Flink->Blink = entry->Blink;

    entry->Flink = NULL;
    entry->Blink = NULL;
}