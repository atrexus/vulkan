#include "imports.h"
#include "syscalls.h"
#include "out.h"

#define ALIGN(Size, Alignment) (((Size) + (Alignment)-1) & ~((Alignment)-1))

_Success_(return > 0)
static DWORD
RvaToOffset(_In_ PIMAGE_NT_HEADERS NtHeaders, _In_ DWORD Rva);

_Success_(return != NULL)
static PIMAGE_SECTION_HEADER
AddSection(_In_ PIMAGE_NT_HEADERS NtHeaders, _In_ PCHAR Name, _In_ DWORD Characteristics);

_Success_(return)
static BOOL
RebuildImportTable(_In_ PDUMPER Dumper, _In_ PBYTE ImageBase, _In_ PIMAGE_SECTION_HEADER SectionHeader);

_Success_(return)
static BOOL
RebuildImportTableForModule(
    _In_ PDUMPER Dumper,
    _In_ PBYTE ImageBase,
    _In_ HMODULE Module,
    _In_ PIMAGE_SECTION_HEADER ImageDataSection);

_Success_(return)
static BOOL
ResolveImportsDynamically(_In_ PDUMPER Dumper, _In_ PBYTE *OriginalImage);

_Success_(return)
static BOOL
ResizeImage(_Inout_ PBYTE *OriginalImage, _In_ SIZE_T NewSize);

// =====================================================================================================================
// Public functions
// =====================================================================================================================

_Success_(return)
BOOL
ResolveImports(_In_ PDUMPER Dumper, _In_ PBYTE *OriginalImage)
{
    PIMAGE_DOS_HEADER DosHeader;
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_OPTIONAL_HEADER OptionalHeader;
    PIMAGE_DATA_DIRECTORY DataDirectory;
    PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor;
    PBYTE ImageBase;
    DWORD ImportDirectoryRva, ImportDirectorySize;

    //
    // Ensure that the original image is not NULL.
    //
    if (OriginalImage == NULL)
    {
        return FALSE;
    }

    ImageBase = *OriginalImage;

    //
    // Get all of the headers.
    //
    DosHeader = (PIMAGE_DOS_HEADER)ImageBase;
    NtHeaders = RVA2VA(PIMAGE_NT_HEADERS, ImageBase, DosHeader->e_lfanew);
    OptionalHeader = &NtHeaders->OptionalHeader;
    DataDirectory = (PIMAGE_DATA_DIRECTORY)OptionalHeader->DataDirectory;

    //
    // Get the import directory RVA and size.
    //
    ImportDirectoryRva = DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    ImportDirectorySize = DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;

    //
    // Get the first import descriptor.
    //
    ImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(ImageBase + RvaToOffset(NtHeaders, ImportDirectoryRva));

    //
    // If the import directory RVA is zero, or the import descriptor is NULL, then the module's imports are corrupted.
    // We need to resolve the imports dynamically.
    //
    if (ImportDirectoryRva == 0 || ImportDescriptor->OriginalFirstThunk == 0)
    {
        info("Resolving imports dynamically.");

        return ResolveImportsDynamically(Dumper, &ImageBase);
    }

    return TRUE;
}

// =====================================================================================================================
// Private functions
// =====================================================================================================================

_Success_(return)
static BOOL
ResolveImportsDynamically(_In_ PDUMPER Dumper, _In_ PBYTE *OriginalImage)
{
    PBYTE ImageBase;
    PIMAGE_DOS_HEADER DosHeader;
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_OPTIONAL_HEADER OptionalHeader;
    PIMAGE_DATA_DIRECTORY DataDirectory;
    PIMAGE_SECTION_HEADER SectionHeader;
    IMAGE_DATA_DIRECTORY ImportDataDirectory;

    ImageBase = *OriginalImage;

    //
    // Get all of the headers.
    //
    DosHeader = (PIMAGE_DOS_HEADER)ImageBase;
    NtHeaders = RVA2VA(PIMAGE_NT_HEADERS, ImageBase, DosHeader->e_lfanew);
    OptionalHeader = &NtHeaders->OptionalHeader;
    DataDirectory = (PIMAGE_DATA_DIRECTORY)OptionalHeader->DataDirectory;

    //
    // Get the import data directory.
    //
    ImportDataDirectory = DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

    //
    // Add a new section to the image. We will not be updating the size of the image, this will happen when we write the
    // imports to the image.
    //
    SectionHeader =
        AddSection(NtHeaders, ".vulkan", IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE);

    //
    // Resize the image to include the new section.
    //
    *OriginalImage = realloc(*OriginalImage, NtHeaders->OptionalHeader.SizeOfImage);
    ImageBase = *OriginalImage;

    //
    // Refresh all of the headers after resizing the image.
    //
    DosHeader = (PIMAGE_DOS_HEADER)ImageBase;
    NtHeaders = RVA2VA(PIMAGE_NT_HEADERS, ImageBase, DosHeader->e_lfanew);
    SectionHeader = IMAGE_FIRST_SECTION(NtHeaders) + NtHeaders->FileHeader.NumberOfSections - 1;

    //
    // Rebuild the import table.
    //
    if (!RebuildImportTable(Dumper, ImageBase, SectionHeader))
    {
        return FALSE;
    }

    //
    // Refresh all of the headers after rebuilding the import table.
    //
    DosHeader = (PIMAGE_DOS_HEADER)ImageBase;
    NtHeaders = RVA2VA(PIMAGE_NT_HEADERS, ImageBase, DosHeader->e_lfanew);
    SectionHeader = IMAGE_FIRST_SECTION(NtHeaders) + NtHeaders->FileHeader.NumberOfSections - 1;

    //
    // Update the import data directory.
    //
    ImportDataDirectory.VirtualAddress = SectionHeader->VirtualAddress;
    ImportDataDirectory.Size = SectionHeader->SizeOfRawData;

    return TRUE;
}

_Success_(return)
static BOOL
RebuildImportTable(_In_ PDUMPER Dumper, _In_ PBYTE ImageBase, _In_ PIMAGE_SECTION_HEADER SectionHeader)
{
    HMODULE Modules[1024];
    DWORD Needed, i;
    PCHAR ModuleName[MAX_PATH];

    //
    // Get the modules that are loaded in the process.
    //
    if (!EnumProcessModules(Dumper->Process, Modules, sizeof(Modules), &Needed))
    {
        error("Failed to enumerate the process modules. Error: %lu", GetLastError());

        return FALSE;
    }

    ModuleName[0] = '\0';

    //
    // Loop through all of the modules and rebuild the import table.
    //
    for (i = 1; i < Needed / sizeof(HMODULE); i++)
    {
        if (!GetModuleBaseName(Dumper->Process, Modules[i], ModuleName, sizeof(ModuleName)))
        {
            error("Failed to get the module base name. Error: %lu", GetLastError());

            return FALSE;
        }

        //
        // If the module is the current module, then we will skip it.
        //
        if (lstrcmpiA(ModuleName, Dumper->ProcessName) == 0)
        {
            continue;
        }

        debug("Rebuilding the import table for module \"%s\".", ModuleName);

        if (!RebuildImportTableForModule(Dumper, ImageBase, Modules[i], SectionHeader))
        {
            continue;
        }
    }

    return TRUE;
}

_Success_(return)
static BOOL
RebuildImportTableForModule(
    _In_ PDUMPER Dumper,
    _In_ PBYTE ImageBase,
    _In_ HMODULE Module,
    _In_ PIMAGE_SECTION_HEADER ImageDataSection)
{
    BOOL Result;
    PBYTE Buffer;
    MODULEINFO ModuleInfo;
    NTSTATUS Status;
    PIMAGE_DOS_HEADER DosHeader;
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_OPTIONAL_HEADER OptionalHeader;
    ULONG_PTR ExportDirectoryRva;
    PIMAGE_EXPORT_DIRECTORY ExportDirectory;
    PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor;
    PDWORD Names, Functions;
    PWORD Ordinals;
    PCHAR FunctionName;
    DWORD FunctionRva;
    PVOID FunctionAddress;
    DWORD i;

    Result = FALSE;

    if (!GetModuleInformation(Dumper->Process, Module, &ModuleInfo, sizeof(MODULEINFO)))
    {
        error("Failed to get the module information. Error: %lu", GetLastError());

        return FALSE;
    }

    //
    // Allocate a buffer with one page of byte (this is to contain the headers of the module).
    //
    Buffer = (PBYTE)malloc(ModuleInfo.SizeOfImage);

    if (!Buffer)
    {
        error("Failed to allocate memory for the module headers.");

        return FALSE;
    }

    //
    // Read the module into the buffer.
    //
    Status = NtReadVirtualMemory(Dumper->Process, ModuleInfo.lpBaseOfDll, Buffer, ModuleInfo.SizeOfImage, NULL);

    //
    // Sometimes the module is not readable, so we will skip it. This is not an error.
    //
    if (!NT_SUCCESS(Status))
    {
        goto Exit;
    }

    //
    // Get the headers of the module.
    //
    DosHeader = (PIMAGE_DOS_HEADER)Buffer;
    NtHeaders = RVA2VA(PIMAGE_NT_HEADERS, Buffer, DosHeader->e_lfanew);
    OptionalHeader = &NtHeaders->OptionalHeader;

    //
    // Get the export directory RVA.
    //
    ExportDirectoryRva = OptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

    //
    // If the export directory RVA is zero, then the module doesn't export any functions.
    //
    if (!ExportDirectoryRva)
    {
        Result = TRUE;

        goto Exit;
    }

    //
    // Get the export directory.
    //
    ExportDirectory = RVA2VA(PIMAGE_EXPORT_DIRECTORY, Buffer, ExportDirectoryRva);

    //
    // Get the names, functions, and ordinals.
    //
    Names = RVA2VA(PDWORD, Buffer, ExportDirectory->AddressOfNames);
    Functions = RVA2VA(PDWORD, Buffer, ExportDirectory->AddressOfFunctions);
    Ordinals = RVA2VA(PWORD, Buffer, ExportDirectory->AddressOfNameOrdinals);

    //
    // Loop through all of the exported functions.
    //
    for (i = 0; i < ExportDirectory->NumberOfNames; i++)
    {
        //
        // Get the function name.
        //
        FunctionName = RVA2VA(PCHAR, Buffer, Names[i]);
        FunctionRva = Functions[Ordinals[i]];
        FunctionAddress = RVA2VA(PVOID, ImageBase, FunctionRva);

        debug("Rebuilding the import table for function \"%s\".", FunctionName);
    }

Exit:
    free(Buffer);

    return Result;
}

_Success_(return)
static BOOL
ResizeImage(_Inout_ PBYTE *OriginalImage, _In_ SIZE_T NewSize)
{
    PBYTE ResizedImage;

    //
    // Reallocate the original image.
    //
    ResizedImage = (PBYTE)realloc(*OriginalImage, NewSize);

    if (!ResizedImage)
    {
        error("Failed to reallocate memory for the image.");

        return FALSE;
    }

    //
    // Assign the resized image back to the original pointer.
    //
    *OriginalImage = ResizedImage;

    return TRUE;
}


_Success_(return > 0)
static DWORD
RvaToOffset(_In_ PIMAGE_NT_HEADERS NtHeaders, _In_ DWORD Rva)
{
    PIMAGE_SECTION_HEADER SectionHeader;
    DWORD i;

    //
    // Get the first section header.
    //
    SectionHeader = IMAGE_FIRST_SECTION(NtHeaders);

    //
    // Loop through all of the sections.
    //
    for (i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++)
    {
        //
        // Check if the RVA is within the section.
        //
        if (Rva >= SectionHeader->VirtualAddress &&
            Rva < SectionHeader->VirtualAddress + SectionHeader->Misc.VirtualSize)
        {
            return Rva - SectionHeader->VirtualAddress + SectionHeader->PointerToRawData;
        }

        //
        // Get the next section header.
        //
        SectionHeader++;
    }

    return 0;
}

_Success_(return != NULL)
static PIMAGE_SECTION_HEADER
AddSection(_In_ PIMAGE_NT_HEADERS NtHeaders, _In_ PCHAR Name, _In_ DWORD Characteristics)
{
    PIMAGE_SECTION_HEADER SectionHeader;

    //
    // Get the last section header.
    //
    SectionHeader = IMAGE_FIRST_SECTION(NtHeaders) + NtHeaders->FileHeader.NumberOfSections - 1;

    //
    // Add a new section to the image.
    //
    SectionHeader++;
    SectionHeader->Misc.VirtualSize = NtHeaders->OptionalHeader.SectionAlignment;
    SectionHeader->SizeOfRawData =
        ALIGN(NtHeaders->OptionalHeader.SectionAlignment, NtHeaders->OptionalHeader.FileAlignment);
    SectionHeader->VirtualAddress =
        ALIGN(NtHeaders->OptionalHeader.SizeOfImage, NtHeaders->OptionalHeader.SectionAlignment);
    SectionHeader->PointerToRawData =
        ALIGN(NtHeaders->OptionalHeader.SizeOfImage, NtHeaders->OptionalHeader.FileAlignment);
    SectionHeader->Characteristics = Characteristics;

    //
    // Add the name to the section header.
    //
    strcpy((PCHAR)SectionHeader->Name, Name);

    //
    // Increase the size of the image.
    //
    NtHeaders->OptionalHeader.SizeOfImage += SectionHeader->Misc.VirtualSize;

    NtHeaders->FileHeader.NumberOfSections++;

    debug("Added a new section \"%s\" to the image.", SectionHeader->Name);

    return SectionHeader;
}