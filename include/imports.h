//
// Description: This file contains the declarations of the functions that are used to manipulate the imports of the
// dumped image.
//

#pragma once

#include "dumper.h"

typedef struct _IMPORT_TABLE_ENTRY
{
    IMAGE_IMPORT_DESCRIPTOR Descriptor; // The import descriptor.
    PCHAR ModuleName;                   // The name of the module.
    PIMAGE_THUNK_DATA ThunkData;        // The array of thunk data.


} IMPORT_TABLE_ENTRY, *PIMPORT_TABLE_ENTRY;

//
// Resolves the import table of the specified module. The original image contains the raw data of the module.
//
_Success_(return)
BOOL
ResolveImports(_In_ PDUMPER Dumper, _Inout_ PBYTE *OriginalImage);