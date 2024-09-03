#include "sections.h"
#include "syscalls.h"
#include "out.h"

#define ALIGN_UP(x, align) (((x) + ((align)-1)) & ~((align)-1))

_Success_(return)
static BOOL
IsPossiblyEncrypted(_In_ PIMAGE_SECTION_HEADER SectionHeader);

_Success_(return)
static BOOL
DecryptSection(_In_ PDUMPER Dumper, _In_ PIMAGE_SECTION_HEADER SectionHeader, _In_ PBYTE ImageBase);

// =====================================================================================================================
// Public functions
// =====================================================================================================================

_Success_(return)
BOOL
ResolveSections(_In_ PDUMPER Dumper, _In_ PBYTE *OriginalImage)
{
    PIMAGE_DOS_HEADER DosHeader;
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_OPTIONAL_HEADER OptionalHeader;
    PIMAGE_SECTION_HEADER SectionHeader;
    PBYTE ImageBase;
    DWORD i, ImagePtr;
    NTSTATUS Status;
    PVOID BaseAddress;

    //
    // Ensure that the original image is not NULL.
    //
    if (OriginalImage == NULL)
    {
        return FALSE;
    }

    //
    // Get the base of the image.
    //
    ImageBase = *OriginalImage;

    //
    // Get all of the headers.
    //
    DosHeader = (PIMAGE_DOS_HEADER)ImageBase;
    NtHeaders = RVA2VA(PIMAGE_NT_HEADERS, ImageBase, DosHeader->e_lfanew);
    OptionalHeader = &NtHeaders->OptionalHeader;
    ImagePtr = OptionalHeader->SizeOfHeaders;
    SectionHeader = IMAGE_FIRST_SECTION(NtHeaders);

    //
    // Loop through all of the sections.
    //
    for (i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++, SectionHeader++)
    {
        //
        // Realign the virtual size of the section.
        //
        SectionHeader->Misc.VirtualSize =
            ALIGN_UP(SectionHeader->Misc.VirtualSize, NtHeaders->OptionalHeader.SectionAlignment);

        //
        // Calculate the base address of the section.
        //
        BaseAddress = RVA2VA(PVOID, OptionalHeader->ImageBase, SectionHeader->VirtualAddress);

        //
        // If the section is possibly encrypted, then we will decrypt it.
        //
        if (IsPossiblyEncrypted(SectionHeader))
        {
            if (Dumper->DecryptionFactor && !DecryptSection(Dumper, SectionHeader, ImageBase))
            {
                return FALSE;
            }
        }
        else
        {
            //
            // If the section is not encrypted, then we will simply read it.
            //
            Status = NtReadVirtualMemory(
                Dumper->Process,
                BaseAddress,
                ImageBase + SectionHeader->PointerToRawData,
                SectionHeader->SizeOfRawData,
                NULL);

            if (!NT_SUCCESS(Status))
            {
                error("Failed to read memory region at 0x%08X (0x%08X)", SectionHeader->VirtualAddress, Status);
                break;
            }
        }

        info("Read section %s at 0x%p", SectionHeader->Name, BaseAddress);
    }

    return TRUE;
}

// =====================================================================================================================
// Private functions
// =====================================================================================================================

_Success_(return)
static BOOL
IsPossiblyEncrypted(_In_ PIMAGE_SECTION_HEADER SectionHeader)
{
    //
    // If the section is named ".text" then it is possibly encrypted.
    //
    return lstrcmpA((PCHAR)SectionHeader->Name, ".text") == 0 || lstrcmpA((PCHAR)SectionHeader->Name, ".vmp0") == 0;
}

_Success_(return)
static BOOL
DecryptSection(_In_ PDUMPER Dumper, _In_ PIMAGE_SECTION_HEADER SectionHeader, _In_ PBYTE ImageBase)
{
    DWORD PageRva, PagesDecrypted, PagesToDecrypt;
    SIZE_T PageIndex, TotalPageCount;
    PVOID BaseAddress;
    PBYTE BufferPtr;
    MEMORY_BASIC_INFORMATION MemoryInfo;
    NTSTATUS Status;
    PBOOL PagesList;
    PCHAR SectionName;

    SectionName = (PCHAR)SectionHeader->Name;

    //
    // If this section is not named ".text" then the decryption factor is not applied.
    //
    PagesDecrypted = 0;
    TotalPageCount = SectionHeader->SizeOfRawData / PAGE_SIZE;
    PagesToDecrypt =
        TotalPageCount * (!lstrcmpA((PCHAR)SectionHeader->Name, ".text") ? Dumper->DecryptionFactor : 1.0f);

    //
    // Allocate a list of booleans to keep track of the decrypted pages.
    //
    PagesList = calloc(TotalPageCount, sizeof(BOOL));

    if (!PagesList)
    {
        error("Failed to allocate memory for the pages list");
        return FALSE;
    }

    //
    // Set the entire page list to FALSE.
    //
    for (PageIndex = 0; PageIndex < TotalPageCount; ++PageIndex)
    {
        PagesList[PageIndex] = FALSE;
    }

    PageRva = 0;

DecryptionRoutine:

    //
    // Now we will iterate over all pages in the provided section and attempt to decrypt them.
    //
    for (PageIndex = 0; PageIndex < TotalPageCount; ++PageIndex)
    {
        //
        // Calculate the base address of the current page.
        //
        PageRva = PageIndex * PAGE_SIZE;
        BaseAddress = RVA2VA(PVOID, Dumper->ModuleInfo.lpBaseOfDll, SectionHeader->VirtualAddress + PageRva);
        BufferPtr = RVA2VA(PBYTE, ImageBase, SectionHeader->PointerToRawData + PageRva);

        //
        // If the page is already decrypted, then skip it.
        //
        if (PagesList[PageIndex])
        {
            continue;
        }

        //
        // Query the protection of the current page.
        //
        if (!VirtualQueryEx(Dumper->Process, BaseAddress, &MemoryInfo, sizeof(MemoryInfo)))
            continue;

        //
        // If the page is NO_ACCESS, then we will skip it.
        //
        if (MemoryInfo.Protect & PAGE_NOACCESS)
        {
            //
            // Fill it with nop instructions.
            //
            memset(BufferPtr, 0x90, PAGE_SIZE);
            continue;
        }

        //
        // Now we can read the content of the page and update the buffer.
        //
        Status = NtReadVirtualMemory(Dumper->Process, BaseAddress, BufferPtr, PAGE_SIZE, NULL);

        if (!NT_SUCCESS(Status))
        {
            free(PagesList);

            error("Failed to read memory region at 0x%p (0x%08X)", BaseAddress, Status);
            return FALSE;
        }

        //
        // Mark the page as decrypted.
        //
        PagesList[PageIndex] = TRUE;
        PagesDecrypted++;

        if (Dumper->DecryptionFactor)
        {
            info(
                "Decrypted page in %s at 0x%p (%lu/%lu = %.2f%%)",
                SectionName,
                BaseAddress,
                PagesDecrypted,
                PagesToDecrypt,
                (FLOAT)PagesDecrypted / PagesToDecrypt * 100.0f);
        }
    }

    //
    // We want to exit only if we have decrypted the target amount.
    //
    if (PagesDecrypted < PagesToDecrypt)
    {
        goto DecryptionRoutine;
    }

    free(PagesList);
    return TRUE;
}