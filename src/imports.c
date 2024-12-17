#include "imports.h"
#include "syscalls.h"
#include "out.h"

_Success_(return)
static BOOL
AddImportSection(_In_ PDUMPER Dumper, _Inout_ PBYTE *OriginalImage, _Out_ PIMAGE_SECTION_HEADER *Header);

_Success_(return)
static BOOL
GetLoadedModules(_In_ PDUMPER Dumper, _Out_ HMODULE **LoadedModules, _Out_ PSIZE_T Size);

_Success_(return)
BOOL
BuildImportDirectory(
    _In_ PDUMPER Dumper,
    _In_ HMODULE *Modules,
    _In_ SIZE_T ModuleCount,
    _Inout_ PBYTE ImportSectionBase);

// =====================================================================================================================
// Public functions
// =====================================================================================================================

_Success_(return)
BOOL
ResolveImports(_In_ PDUMPER Dumper, _Inout_ PBYTE *OriginalImage)
{
    HMODULE *Modules;
    SIZE_T ModuleCount;
    PIMAGE_SECTION_HEADER ImportSectionHeader;
    DWORD ImportSectionBase;
    PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor;
    DWORD i;

    //
    // Add the import section to the image.
    //
    if (!AddImportSection(Dumper, OriginalImage, &ImportSectionHeader))
    {
        error("Failed to add the import section to the image");
        return FALSE;
    }

    ImportSectionBase = ImportSectionHeader->PointerToRawData;

    info(
        "Added import section \"%s\" (0x%08X) to the image",
        (PCHAR)ImportSectionHeader->Name,
        ImportSectionHeader->VirtualAddress);

    //
    // Get the loaded modules of the target process.
    //
    if (!GetLoadedModules(Dumper, &Modules, &ModuleCount))
    {
        error("Failed to get the loaded modules of the target process");
        return FALSE;
    }

    for (i = 0; i < ModuleCount; i++)
    {
        ImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(*OriginalImage + ImportSectionBase);

        THUNK
    }

    return TRUE;
}

// =====================================================================================================================
// Private functions
// =====================================================================================================================
_Success_(return)
static BOOL
AddImportSection(_In_ PDUMPER Dumper, _Inout_ PBYTE *OriginalImage, _Out_ PIMAGE_SECTION_HEADER *Header)
{
    PIMAGE_DOS_HEADER DosHeader;
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_SECTION_HEADER LastSectionHeader;
    IMAGE_SECTION_HEADER ImportSectionHeader;
    PBYTE ImportSectionHeaderBase;

    //
    // Ensure that the original image is not NULL.
    //
    if (OriginalImage == NULL)
    {
        return FALSE;
    }

    //
    // Get the headers of the image.
    //
    DosHeader = (PIMAGE_DOS_HEADER)*OriginalImage;
    NtHeaders = RVA2VA(PIMAGE_NT_HEADERS, *OriginalImage, DosHeader->e_lfanew);
    LastSectionHeader = IMAGE_FIRST_SECTION(NtHeaders) + NtHeaders->FileHeader.NumberOfSections - 1;

    //
    // Get the base of the import section header.
    //
    ImportSectionHeaderBase = RVA2VA(PBYTE, LastSectionHeader, sizeof(IMAGE_SECTION_HEADER));

    //
    // Lets check to see if we need to allocate more space for the new section header (we do this anyways).
    //
    if (ImportSectionHeaderBase + sizeof(IMAGE_SECTION_HEADER) >
        *OriginalImage + NtHeaders->OptionalHeader.SizeOfHeaders)
    {
        info("Need to allocate more space for the new section header");
    }

    //
    // Zero out the import section header.
    //
    ZeroMemory(&ImportSectionHeader, sizeof(IMAGE_SECTION_HEADER));

    //
    // Set the name of the import section header.
    //
    strcpy_s((PCHAR)ImportSectionHeader.Name, sizeof(ImportSectionHeader.Name), ".vulkan");

    //
    // Set the virtual address and pointer to raw of the import section header based on the last section header.
    //
    ImportSectionHeader.VirtualAddress = LastSectionHeader->VirtualAddress + LastSectionHeader->Misc.VirtualSize;
    ImportSectionHeader.PointerToRawData = LastSectionHeader->PointerToRawData + LastSectionHeader->SizeOfRawData;

    //
    // Set the characteristics of the import section header.
    //
    ImportSectionHeader.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;

    //
    // Copy the import section header to the image.
    //
    memcpy(ImportSectionHeaderBase, &ImportSectionHeader, sizeof(IMAGE_SECTION_HEADER));

    //
    // Update the number of sections in the image.
    //
    NtHeaders->FileHeader.NumberOfSections++;

    //
    // Set the output header.
    //
    *Header = ImportSectionHeaderBase;

    return TRUE;
}

_Success_(return)
static BOOL
GetLoadedModules(_In_ PDUMPER Dumper, _Out_ HMODULE **LoadedModules, _Out_ PSIZE_T Size)
{
    HMODULE Modules[1024];
    DWORD Needed;
    DWORD i;

    //
    // Enumerate the loaded modules of the target process.
    //
    if (!EnumProcessModules(Dumper->Process, Modules, sizeof(Modules), &Needed))
    {
        return FALSE;
    }

    //
    // Update the size of the loaded modules.
    //
    *Size = Needed / sizeof(HMODULE);

    //
    // Allocate memory for the loaded modules.
    //
    *LoadedModules = (HMODULE *)malloc(*Size * sizeof(HMODULE));

    if (*LoadedModules == NULL)
    {
        return FALSE;
    }

    //
    // Copy the loaded modules to the output.
    //
    for (i = 0; i < *Size; ++i)
    {
        (*LoadedModules)[i] = Modules[i];
    }

    return TRUE;
}

_Success_(return)
BOOL
BuildImportDirectory(
    _In_ PDUMPER Dumper,
    _In_ HMODULE *Modules,
    _In_ SIZE_T ModuleCount,
    _Inout_ PBYTE ImportSectionBase)
{
    PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor;
    SIZE_T i;

    ImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)ImportSectionBase;

    for (i = 0; i < ModuleCount; i++)
    {
        continue;
    }

    //
    // Null-terminate the import descriptor array.
    //
    ZeroMemory(ImportDescriptor, sizeof(IMAGE_IMPORT_DESCRIPTOR));

    return TRUE;
}