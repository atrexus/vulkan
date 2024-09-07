#include <Windows.h>
#include <shellapi.h>

#include "dumper.h"
#include "out.h"

//
// Prints the usage of the application.
//
static VOID
Usage();

//
// Enables a token privilege for the current process.
//
_Success_(return)
static BOOL
EnableTokenPrivilege(_In_ LPCTSTR Privilege);

//
// Macro that helps define command line arguments.
//
#define CHECK_ARGUMENTS() \
    if (i + 1 >= nArgs) \
    { \
        error("Missing argument for option %ws", szArglist[i]); \
        Usage(); \
        return EXIT_FAILURE; \
    }

//
// The entry point of the application. This is where command-line arguments are parsed and the dumper is launched. Refer
// to the available options below:
//
//  Options:
//      * -p <name> - The name of the target process to dump.
//      * -o <path> - The output directory where the dump will be saved (default: current directory).
//      * --decrypt <factor> - Amount (%) of no access pages to have decrypted before dumping
//  Flags:
//      * -D - Enable debug mode.
//
int _cdecl main()
{
    LPWSTR *szArglist;
    int nArgs;
    LPWSTR OutputPath, Name;
    FLOAT DecryptFactor;
    DUMPER Dumper;
    BOOL DebugMode;

    szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);

    if (!szArglist)
    {
        error("CommandLineToArgvW failed with error %lu", GetLastError());
        return EXIT_FAILURE;
    }
   
    Name = NULL;
    DebugMode = FALSE;
<<<<<<< Updated upstream
    DecryptFactor = 0.60f;
=======
    DecryptFactor = 1.0f;
>>>>>>> Stashed changes
    OutputPath = L".";

    for (int i = 1; i < nArgs; i++)
    {
        if (lstrcmpW(szArglist[i], L"--decrypt") == 0)
        {
            CHECK_ARGUMENTS();

            DecryptFactor = wcstof(szArglist[i + 1], NULL);
            i++;

            if (DecryptFactor < 0.0f || DecryptFactor > 1.0f)
            {
                error("Invalid value for --decrypt flag. Must be between 0.0 and 1.0");
                Usage();
                return EXIT_FAILURE;
            }
        }
        else if (lstrcmpW(szArglist[i], L"-o") == 0)
        {
            CHECK_ARGUMENTS();

            OutputPath = szArglist[i + 1];
            i++;
        }
        else if (lstrcmpW(szArglist[i], L"-p") == 0)
        {
            CHECK_ARGUMENTS();

            Name = szArglist[i + 1];
            i++;
        }
        else if (lstrcmpW(szArglist[i], L"-D") == 0)
        {
            DebugMode = TRUE;
        }
        else
        {
            error("Unknown option: %ws", szArglist[i]);
            Usage();
            return EXIT_FAILURE;
        }
    }

    if (!Name)
    {
        error("Missing argument for option -p");
        Usage();
        return EXIT_FAILURE;
    }

    //
    // Enable the SeDebugPrivilege.
    //
    if (DebugMode && !EnableTokenPrivilege(SE_DEBUG_NAME))
    {
        error("Failed to enable SeDebugPrivilege");
        return 1;
    }

    //
    // Create a new dumper object.
    //
    if (!DumperCreate(&Dumper, Name, OutputPath, DecryptFactor))
    {
        return EXIT_FAILURE;
    }

    //
    // Dump the target process memory to disk.
    //
    if (!DumperDumpToDisk(&Dumper))
    {
        return EXIT_FAILURE;
    }

    //
    // Destroys the dumper object.
    //
    return DumperDestroy(&Dumper);
}

VOID
Usage()
{
    fprintf(stdout, "Usage: dumper [options] <pid>\n");
    fprintf(stdout, "Options:\n");
    fprintf(stdout, "  -p <name>            The name of the target process to dump.\n");
    fprintf(
        stdout,
        "  -o <path>            The output directory where the dump will be saved (default: \".\").\n");
    fprintf(stdout, "  --decrypt <factor>   Fraction of no access pages to have decrypted before dumping (Default: 1).\n");
    fprintf(stdout, "Flags:\n");
    fprintf(stdout, "  -D                   Enable debug mode (Default: false).\n");
}

_Success_(return)
static BOOL
EnableTokenPrivilege(_In_ LPCTSTR Privilege)
{
    HANDLE Token;
    TOKEN_PRIVILEGES TokenPrivileges;

    Token = NULL;

    //
    // Zero out the token privileges structure.
    //
    ZeroMemory(&TokenPrivileges, sizeof(TOKEN_PRIVILEGES));

    //
    // Get a token for this process.
    //
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &Token))
    {
        return FALSE;
    }

    //
    // Get the LUID for the privilege.
    //
    if (LookupPrivilegeValue(NULL, Privilege, &TokenPrivileges.Privileges[0].Luid))
    {
        TokenPrivileges.PrivilegeCount = 1;
        TokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        //
        // Set the privilege for this process.
        //
        return AdjustTokenPrivileges(Token, FALSE, &TokenPrivileges, 0, (PTOKEN_PRIVILEGES)NULL, 0);
    }

    return FALSE;
}