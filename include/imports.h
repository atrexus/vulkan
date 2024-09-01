//
// Description: This file contains the declarations of the functions that are used to manipulate the imports of the
// dumped image.
//

#pragma once

#include "dumper.h"

//
// Resolves the imports of the specified module. The original image contains the raw data of the module.
//
_Success_(return)
BOOL
ResolveImports(_In_ PDUMPER Dumper, _In_ PBYTE *OriginalImage);