#include <stdio.h>
#include <windows.h>
#include <string.h>
#include <stdbool.h>

#define ERROR_EXIT_CODE 20000

#define TARGET_EXE_RESOURCE 1

/**
 * @brief full path to the current exe file
 * @return path that must be freed later or 0 if an error occures
 */
wchar_t* getExePath()
{
    // get our executable name
    DWORD sz = MAX_PATH;
    LPTSTR exe = malloc(sz * sizeof(TCHAR));
    if (GetModuleFileName(0, exe, sz) == sz) {
        free(exe);
        sz = MAX_PATH * 10;
        exe = malloc(sz * sizeof(TCHAR));
        if (GetModuleFileName(0, exe, sz) == sz) {
            exe = 0;
        }
    }

    return exe;
}

typedef struct {
    HANDLE hUpdateRes;
} EnumResourcesData;

WINBOOL CALLBACK enumResources(HMODULE hModule, LPCWSTR lpType, LPWSTR lpName,
        LONG_PTR lParam)
{
    EnumResourcesData* data = (EnumResourcesData*) lParam;

    HRSRC hRes = FindResource(hModule, lpName, lpType);

    HRSRC hResLoad = NULL;
    if (hRes != NULL) {
        //wprintf(L"Loading the icon resource\n");

        // Load the ICON into global memory.
        hResLoad = (HRSRC)LoadResource(hModule, hRes);
    }

    // pointer to resource data
    char *lpResLock = 0;
    if (hResLoad != NULL) {
        //wprintf(L"Locking the icon resource\n");

        // Lock the ICON into global memory.
        lpResLock = (char*)LockResource(hResLoad);
    }

    if (lpResLock) {
        //wprintf(L"Updating the icon resource\n");

        UpdateResource(data->hUpdateRes,
                lpType,
                lpName,
                MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                lpResLock,
                SizeofResource(hModule, hRes));
    }

    return TRUE;
}

void printError(DWORD err) {
    wprintf(L"Error %d\n", err);
}

/**
 * @brief copies the icon from one file to another
 *
 * @param hUpdateRes result of BeginUpdateResource
 * @param lpszSourceFile file with the icon
 * @param copyIcon should the icons and icon groups be copied
 * @param copyVersion should the version information be copied
 * @param copyManifest should the executable manifest be copied
 */
void copyResources(HANDLE hUpdateRes, LPCWSTR lpszSourceFile, bool copyIcon,
        bool copyVersion, bool copyManifest)
{
    //wprintf(L"Copying the icon\n");
    //wprintf(lpszSourceFile);

    // Load the source exe from where we need the icon
    HMODULE hSrcExe = LoadLibraryEx(lpszSourceFile, NULL,
            LOAD_LIBRARY_AS_IMAGE_RESOURCE);

    DWORD err = GetLastError();
    if (err) {
        printError(err);
    }

    if (hSrcExe != NULL) {
        // wprintf(L"Searching for the icon\n");

        EnumResourcesData data = {0};
        data.hUpdateRes = hUpdateRes;

        if (copyIcon) {
            EnumResourceNames(hSrcExe, MAKEINTRESOURCE(RT_ICON), enumResources,
                    (LONG_PTR) &data);
            EnumResourceNames(hSrcExe, MAKEINTRESOURCE(RT_GROUP_ICON),
                    enumResources, (LONG_PTR) &data);
        }
        if (copyVersion) {
            EnumResourceNames(hSrcExe, MAKEINTRESOURCE(RT_VERSION),
                    enumResources, (LONG_PTR) &data);
        }
        if (copyManifest) {
            EnumResourceNames(hSrcExe, MAKEINTRESOURCE(RT_MANIFEST),
                    enumResources, (LONG_PTR) &data);
        }
    }

    if (hSrcExe != NULL) {
        FreeLibrary(hSrcExe);
    }
}

int copyExe(wchar_t* exeProxy, wchar_t* target, bool copyIcon,
        bool copyVersion, bool copyManifest)
{
    int ret = 0;

    wchar_t* exe = getExePath();
    if (!exe) {
        ret = ERROR_EXIT_CODE;
        wprintf(L"Cannot determine the name of this executable\n");
    }

    if (!ret) {
        if (!CopyFile(exe, exeProxy, TRUE)) {
            ret = ERROR_EXIT_CODE;
            wprintf(L"Copying the executable failed\n");
        }
    }

    free(exe);

    HANDLE hUpdateRes = 0;
    if (!ret) {
        hUpdateRes = BeginUpdateResource(exeProxy, TRUE);
        if (!hUpdateRes) {
            ret = ERROR_EXIT_CODE;
            wprintf(L"BeginUpdateResource failed\n");
        }
    }

    // set the path to the target .exe file in the resource string
    if (!ret) {
        // Resource strings are stored in groups of 16.
        // Each string is preceeded by its size.
        size_t targetLen = wcslen(target);
        size_t bufSize = (targetLen + 16) * sizeof(wchar_t);
        wchar_t* buf = malloc(bufSize);
        memset(buf, 0, bufSize);
        buf[1] = targetLen;
        wcscpy(buf + 2, target);

        if (!UpdateResource(hUpdateRes,
                RT_STRING,
                MAKEINTRESOURCE(TARGET_EXE_RESOURCE),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                buf,
                bufSize)) {
            ret = ERROR_EXIT_CODE;
            wprintf(L"UpdateResource failed\n");
        }
    }

    if (!ret) {
        copyResources(hUpdateRes, target, copyIcon, copyVersion, copyManifest);
    }

    if (!ret) {
        if (!EndUpdateResource(hUpdateRes, FALSE)) {
            ret = ERROR_EXIT_CODE;
            wprintf(L"EndUpdateResource failed with the error code %d\n",
                    GetLastError());
        }
    }

    return ret;
}

PROCESS_INFORMATION pinfo;

BOOL WINAPI ctrlHandler(DWORD fdwCtrlType)
{
    wprintf(L"ctrlHandler\n");
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
            GenerateConsoleCtrlEvent(CTRL_C_EVENT, pinfo.dwProcessId);
            break;
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pinfo.dwProcessId);
            break;
    }

    WaitForSingleObject(pinfo.hProcess, 1000);

    DWORD ec;
    if (GetExitCodeProcess(pinfo.hProcess, &ec) && (ec == STILL_ACTIVE)) {
        TerminateProcess(pinfo.hProcess, 1);
    }

    return FALSE;
}

int wmain(int argc, wchar_t **argv)
{
    int ret = 0;

    if (argc >= 4 && wcscmp(argv[1], L"exeproxy-copy") == 0) {
        bool copyIcon = false;
        bool copyVersion = false;
        bool copyManifest = false;
        for (int i = 4; i < argc; i++) {
            if (wcscmp(argv[i], L"--copy-icons") == 0)
                copyIcon = true;
            else if (wcscmp(argv[i], L"--copy-version") == 0)
                copyVersion = true;
            else if (wcscmp(argv[i], L"--copy-manifest") == 0)
                copyManifest = true;
        }
        ret = copyExe(argv[2], argv[3], copyIcon, copyVersion, copyManifest);
        return ret;
    }

    wchar_t* exe = getExePath();
    if (!exe) {
        ret = ERROR_EXIT_CODE;
        wprintf(L"Cannot determine the name of this executable\n");
    }

    // extract parameters
    LPTSTR cl = GetCommandLine();
    LPTSTR args = 0;
    if (!ret) {
        if (*cl == L'"') {
            LPTSTR second = wcschr(cl + 1, '"');
            if (!second) {
                args = wcschr(cl, L' ');
                if (!args)
                    args = wcsdup(L"");
                else
                    args = wcsdup(args + 1);
            } else {
                args = wcsdup(second + 1);
            }
        } else {
            args = wcschr(cl, L' ');
            if (!args)
                args = wcsdup(L"");
            else
                args = wcsdup(args + 1);
        }
    }

    wchar_t* target = 0;
    if (!ret) {
        target = malloc(MAX_PATH * sizeof(TCHAR));
        if (LoadString(0, (TARGET_EXE_RESOURCE - 1) * 16 + 1,
                target, MAX_PATH) == 0) {
            ret = ERROR_EXIT_CODE;
            wprintf(L"Cannot load the target executable path from the resource\n");
        }
    }

    // find the name of the target executable
    wchar_t* newExe = 0;
    if (!ret) {
        // is full path to the target executable available?
        if (wcschr(target, L'\\') || wcschr(target, L'/')) {
            newExe = wcsdup(target);
        } else {
            wchar_t* p1 = wcsrchr(exe, L'\\');
            wchar_t* p2 = wcsrchr(exe, L'/');
            wchar_t* p = exe;
            if (p1 < p2)
                p = p2 + 1;
            else if (p1 > p2)
                p = p1 + 1;
            else
                p = exe;

            size_t n = p - exe;
            newExe = malloc((n + wcslen(target) + 1) * sizeof(wchar_t));
            wcsncpy(newExe, exe, n);
            *(newExe + n) = 0;

            wcscat(newExe, target);
        }
    }

    free(target);

    wchar_t* cmdLine = 0;
    if (!ret) {
        cmdLine = malloc((wcslen(newExe) + wcslen(args) + 4) * sizeof(wchar_t));
        wcscpy(cmdLine, L"\"");
        wcscat(cmdLine, newExe);
        wcscat(cmdLine, L"\"");
        if (wcslen(args) > 0) {
            wcscat(cmdLine, L" ");
            wcscat(cmdLine, args);
        }
    }

    if (!ret) {
        STARTUPINFOW startupInfo = {
            sizeof(STARTUPINFO), 0, 0, 0,
            (DWORD) CW_USEDEFAULT, (DWORD) CW_USEDEFAULT,
            (DWORD) CW_USEDEFAULT, (DWORD) CW_USEDEFAULT,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        WINBOOL success = CreateProcess(
                newExe,
                cmdLine,
                0, 0, TRUE,
                CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_PROCESS_GROUP, 0,
                0, &startupInfo, &pinfo);


        if (success) {
            SetConsoleCtrlHandler(ctrlHandler, TRUE);

            WaitForSingleObject(pinfo.hProcess, INFINITE);
            DWORD ec;
            if (GetExitCodeProcess(pinfo.hProcess, &ec))
                ret = ec;
            else
                ret = ERROR_EXIT_CODE;
            CloseHandle(pinfo.hThread);
            CloseHandle(pinfo.hProcess);
        } else {
            wprintf(L"Error starting %ls %ls\n", newExe, args);
            ret = ERROR_EXIT_CODE;
        }
    }
    
    free(cmdLine);
    free(exe);
    free(newExe);
    free(args);

    return ret;
}

