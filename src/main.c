// should the JavaScript be supported?
#define EXE_PROXY_JAVASCRIPT 1

#include <stdio.h>
#include <windows.h>
#include <string.h>
#include <stdbool.h>
#include <shellapi.h>
#include <jni.h>

#ifdef EXE_PROXY_JAVASCRIPT
#include "duktape.h"
#endif

#define ERROR_EXIT_CODE 20000

#define TARGET_EXE_RESOURCE 1

static wchar_t* g_ServiceName = 0;
static SERVICE_STATUS        g_ServiceStatus = {0};
static SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
static HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;
static JavaVM *javaVM = 0;
static JNIEnv *jniEnv = 0;

/**
 * @brief converts a string to UTF-16
 * @param s UTF-8
 * @return new UTF-16 string
 */
static wchar_t* toUTF16(const char* s)
{
    int n = strlen(s);

    int sz = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, n, NULL, 0);
    wchar_t* r = malloc(sz * sizeof(wchar_t));
    sz = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, n, r, sz);
    *(r + sz) = 0;

    return r;
}

/**
 * @brief full path to the current exe file
 * @return path that must be freed later or 0 if an error occures
 */
static wchar_t* getExePath()
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

static WINBOOL CALLBACK enumResources(HMODULE hModule, LPCWSTR lpType, LPWSTR lpName,
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

static void printError(DWORD err) {
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
static void copyResources(HANDLE hUpdateRes, LPCWSTR lpszSourceFile, bool copyIcon,
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

static int copyExe(wchar_t* exeProxy, wchar_t* target, bool copyIcon,
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

static BOOL WINAPI ctrlHandler(DWORD fdwCtrlType)
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

/**
 * @brief executes a program
 * @param cmdLine command line
 * @return exit code
 */
static int exec(wchar_t* cmdLine)
{
    int ret = 0;

    int numArgs;
    LPWSTR* args = CommandLineToArgvW(cmdLine, &numArgs);

    STARTUPINFOW startupInfo = {
        sizeof(STARTUPINFO), 0, 0, 0,
        (DWORD) CW_USEDEFAULT, (DWORD) CW_USEDEFAULT,
        (DWORD) CW_USEDEFAULT, (DWORD) CW_USEDEFAULT,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    WINBOOL success = CreateProcess(
            numArgs > 0 ? args[0] : L"",
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
        wprintf(L"Error starting %ls\n", cmdLine);
        ret = ERROR_EXIT_CODE;
    }

    LocalFree(args);

    return ret;
}

#ifdef EXE_PROXY_JAVASCRIPT

DWORD WINAPI ServiceWorkerThread (LPVOID lpParam)
{
    (void) lpParam;

    wprintf(L"My Sample Service: ServiceWorkerThread: Entry");

    //  Periodically check if the service has been requested to stop
    while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
    {
        /*
         * Perform main service function here
         */

        wprintf(L"My Sample Service: doing work...");

        //  Simulate some work by sleeping
        Sleep(3000);
    }

    wprintf(L"My Sample Service: ServiceWorkerThread: Exit");

    return ERROR_SUCCESS;
}

VOID WINAPI ServiceCtrlHandler (DWORD CtrlCode)
{
    wprintf(L"My Sample Service: ServiceCtrlHandler: Entry");

    //JNIEnv *env;
    //(*javaVM)->AttachCurrentThread(javaVM, (void**) &env, 0);

    switch (CtrlCode)
    {
     case SERVICE_CONTROL_STOP :

        wprintf(L"My Sample Service: ServiceCtrlHandler: SERVICE_CONTROL_STOP Request");

        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
           break;

        /*
         * Perform tasks neccesary to stop the service here
         */

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwWin32ExitCode = 0;
        g_ServiceStatus.dwCheckPoint = 4;

        if (SetServiceStatus (g_StatusHandle, &g_ServiceStatus) == FALSE)
        {
            wprintf(L"My Sample Service: ServiceCtrlHandler: SetServiceStatus returned error");
        }

        // This will signal the worker thread to start shutting down
        SetEvent (g_ServiceStopEvent);

        break;

     default:
         break;
    }

    wprintf(L"My Sample Service: ServiceCtrlHandler: Exit");
}

VOID WINAPI ServiceMain (DWORD argc, LPTSTR *argv)
{
    (void) argc;
    (void) argv;

    //DWORD Status = E_FAIL;

    wprintf(L"My Sample Service: ServiceMain: Entry");

    g_StatusHandle = RegisterServiceCtrlHandler(g_ServiceName, ServiceCtrlHandler);

    if (g_StatusHandle == NULL)
    {
        wprintf(L"My Sample Service: ServiceMain: RegisterServiceCtrlHandler returned error");
        goto EXIT;
    }

    // Tell the service controller we are starting
    ZeroMemory (&g_ServiceStatus, sizeof (g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus (g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        wprintf(L"My Sample Service: ServiceMain: SetServiceStatus returned error");
    }

    /*
     * Perform tasks neccesary to start the service here
     */
    wprintf(L"My Sample Service: ServiceMain: Performing Service Start Operations");

    // Create stop event to wait on later.
    g_ServiceStopEvent = CreateEvent (NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL)
    {
        wprintf(L"My Sample Service: ServiceMain: CreateEvent(g_ServiceStopEvent) returned error");

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;

        if (SetServiceStatus (g_StatusHandle, &g_ServiceStatus) == FALSE)
        {
            wprintf(L"My Sample Service: ServiceMain: SetServiceStatus returned error");
        }
        goto EXIT;
    }

    // Tell the service controller we are started
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus (g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        wprintf(L"My Sample Service: ServiceMain: SetServiceStatus returned error");
    }

    // Start the thread that will perform the main task of the service
    HANDLE hThread = CreateThread (NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

    wprintf(L"My Sample Service: ServiceMain: Waiting for Worker Thread to complete");

    // Wait until our worker thread exits effectively signaling that the service needs to stop
    WaitForSingleObject (hThread, INFINITE);

    wprintf(L"My Sample Service: ServiceMain: Worker Thread Stop Event signaled");


    /*
     * Perform any cleanup tasks
     */
    wprintf(L"My Sample Service: ServiceMain: Performing Cleanup Operations");

    CloseHandle (g_ServiceStopEvent);

    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;

    if (SetServiceStatus (g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        wprintf(L"My Sample Service: ServiceMain: SetServiceStatus returned error");
    }

    EXIT:
    wprintf(L"My Sample Service: ServiceMain: Exit");

    return;
}

static char* replaceChar(char* str, char find, char replace){
    char *current_pos = strchr(str, find);
    while (current_pos) {
        *current_pos = replace;
        current_pos = strchr(current_pos + 1, find);
    }
    return str;
}

static duk_ret_t native_execSync(duk_context *ctx)
{
    const char* cmdLine = duk_safe_to_string(ctx, 0);

    wchar_t* wcmdLine = toUTF16(cmdLine);

    int ec = exec(wcmdLine);

    free(wcmdLine);

    duk_push_number(ctx, ec);

    return 1;  /* one return value */
}

static duk_ret_t native_log(duk_context *ctx)
{
    printf("%s\n", duk_safe_to_string(ctx, 0));

    return 0;
}

static duk_ret_t native_java_service(duk_context *ctx)
{
    wprintf(L"My Sample Service: Main: Entry");

    duk_get_prop_string(ctx, 0, "serviceName");
    const char* serviceName = duk_safe_to_string(ctx, -1);
    duk_pop(ctx);

    free(g_ServiceName);
    g_ServiceName = toUTF16(serviceName);

    SERVICE_TABLE_ENTRY ServiceTable[] =
    {
        {g_ServiceName, (LPSERVICE_MAIN_FUNCTION) ServiceMain},
        {NULL, NULL}
    };

    if (StartServiceCtrlDispatcher (ServiceTable) == FALSE) {
       wprintf(L"My Sample Service: Main: StartServiceCtrlDispatcher returned error");
       printError(GetLastError());
    }

    wprintf(L"My Sample Service: Main: Exit");

    return 0;
}

static duk_ret_t native_exit(duk_context *ctx)
{
    int ec = duk_to_int(ctx, 0);

    exit(ec);
}

static duk_ret_t native_totalmem(duk_context *ctx)
{
    MEMORYSTATUSEX statex;

    statex.dwLength = sizeof(statex);

    GlobalMemoryStatusEx(&statex);

    duk_push_number(ctx, statex.ullTotalPhys);

    return 1;
}

static duk_ret_t native_jvm(duk_context *ctx)
{
    bool err = false;

    duk_get_prop_string(ctx, 0, "jvmDLL");
    const char* dll = duk_safe_to_string(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, 0, "mainClass");
    const char* class = duk_safe_to_string(ctx, -1);
    duk_pop(ctx);

    wchar_t* wdll = toUTF16(dll);

    HMODULE m = LoadLibrary(wdll);
    if (m == NULL) {
        wprintf(L"Error loading the JVM DLL\n");
        printError(GetLastError());
        err = true;
    }

    typedef jint (JNICALL *JNI_createJavaVM)(JavaVM **pvm, JNIEnv **env, void *args);
    JNI_createJavaVM createJavaVM;
    if (!err) {
        createJavaVM = (JNI_createJavaVM) GetProcAddress(m, "JNI_CreateJavaVM");
        if (createJavaVM == NULL) {
            wprintf(L"Error finding the procedure JNI_CreateJavaVM in the DLL\n");
            printError(GetLastError());
        }
    }

    if (!err) {
        // Create the JVM options
        duk_get_prop_string(ctx, 0, "jvmOptions");
        int argc = duk_get_length(ctx, -1);
        JavaVMOption* jvmopt = malloc(argc * sizeof(JavaVMOption));
        for(int i = 0; i < argc; i++) {
            duk_get_prop_index(ctx, -1, i);
            const char* val = duk_safe_to_string(ctx, -1);
            duk_pop(ctx);
            jvmopt[i].optionString = (char*) val;
            jvmopt[i].extraInfo = 0;
        }

        JavaVMInitArgs vmArgs;
        vmArgs.version = JNI_VERSION_1_2;
        vmArgs.nOptions = 1;
        vmArgs.options = jvmopt;
        vmArgs.ignoreUnrecognized = JNI_TRUE;

        // Create the JVM
        long flag = createJavaVM(&javaVM, &jniEnv, &vmArgs);
        if (flag == JNI_ERR) {
            wprintf(L"Error creating VM.\n");
            err = true;
        }

        free(jvmopt);
    }

    jclass jcls;
    if (!err) {
        char* classSlashes = strdup(class);
        replaceChar(classSlashes, '.' , '/');
        jcls = (*jniEnv)->FindClass(jniEnv, classSlashes);
        if (jcls == NULL) {
            (*jniEnv)->ExceptionDescribe(jniEnv);
            err = true;
        }
        free(classSlashes);
    }

    jmethodID methodId;
    if (!err) {
        methodId = (*jniEnv)->GetStaticMethodID(jniEnv, jcls,
                "main", "([Ljava/lang/String;)V");
        if (methodId == NULL) {
            wprintf(L"Cannot find the main() method.\n");
            err = true;
        }
    }

    jclass stringClass;
    if (!err) {
        stringClass = (*jniEnv)->FindClass(jniEnv, "java/lang/String");
        if(stringClass == NULL) {
            wprintf(L"Could not find String class\n");
            err = true;
        }
    }

    if (!err) {
        // Create the run args
        duk_get_prop_string(ctx, 0, "args");
        int argc = duk_get_length(ctx, -1);
        jobjectArray args = (*jniEnv)->NewObjectArray(jniEnv, argc, stringClass, NULL);
        for(int i = 0; i < argc; i++) {
            duk_get_prop_index(ctx, -1, i);
            const char* val = duk_safe_to_string(ctx, -1);
            duk_pop(ctx);
            (*jniEnv)->SetObjectArrayElement(jniEnv, args, i, (*jniEnv)->NewStringUTF(jniEnv, val));
        }

        (*jniEnv)->CallStaticVoidMethod(jniEnv, jcls, methodId, args);
        if ((*jniEnv)->ExceptionCheck(jniEnv)) {
            (*jniEnv)->ExceptionDescribe(jniEnv);
            (*jniEnv)->ExceptionClear(jniEnv);
        }
    }

    if (javaVM)
        (*javaVM)->DestroyJavaVM(javaVM);

    free(wdll);

    return 0;
}

/**
 * @brief converts a string to UTF-8
 * @param s UTF-16
 * @return new UTF-8 string
 */
static char* toUTF8(wchar_t* s)
{
    int n = wcslen(s);

    int sz = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS | WC_NO_BEST_FIT_CHARS, s, n, NULL, 0, NULL, NULL);
    char* r = malloc(sz);
    sz = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS | WC_NO_BEST_FIT_CHARS, s, n, r, sz, NULL, NULL);
    *(r + sz) = 0;

    return r;
}

/**
 * @brief executes JavaScript
 * @param js JavaScript as UTF-8
 * @param executable path to this executable as UTF-8
 * @return error code or 0
 */
static int executeJS(char* js, char* executable)
{
    //wprintf(L"executeJS()");

    int ret = 0;

    duk_context *ctx = duk_create_heap_default();

    {
        // process
        duk_push_object(ctx);  /* push object which will become "process" */

        duk_push_c_function(ctx, native_exit, 1);
        duk_put_prop_string(ctx, -2, "exit");

        duk_push_c_function(ctx, native_jvm, 1);
        duk_put_prop_string(ctx, -2, "loadJVM");

        duk_push_c_function(ctx, native_java_service, 1);
        duk_put_prop_string(ctx, -2, "javaService");

        duk_push_string(ctx, "argv0");
        duk_push_string(ctx, executable);
        duk_put_prop(ctx, -3);

        int numArgs;
        LPTSTR cmdLine = GetCommandLine();
        LPWSTR* args = CommandLineToArgvW(cmdLine, &numArgs);
        duk_push_string(ctx, "argv");
        duk_idx_t arr_idx = duk_push_array(ctx);
        for (int i = 0; i < numArgs; i++) {
            char* val = toUTF8(args[i]);
            duk_push_string(ctx, val);
            duk_put_prop_index(ctx, arr_idx, i);
            free(val);
        }
        duk_put_prop(ctx, -3);
        LocalFree(args);

        duk_put_global_string(ctx, "process");  /* set "process" into the global object */
    }

    {
        // child_process
        duk_push_object(ctx);

        duk_push_c_function(ctx, native_execSync, 1);
        duk_put_prop_string(ctx, -2, "execSync");

        duk_put_global_string(ctx, "child_process");
    }

    {
        // os
        duk_push_object(ctx);

        duk_push_c_function(ctx, native_totalmem, 0);
        duk_put_prop_string(ctx, -2, "totalmem");

        duk_put_global_string(ctx, "os");
    }

    {
        // console
        duk_push_object(ctx);

        duk_push_c_function(ctx, native_log, 1);
        duk_put_prop_string(ctx, -2, "log");

        duk_put_global_string(ctx, "console");
    }

    duk_int_t rc = duk_peval_string(ctx, js);
    if (rc != 0) {
        printf("JavaScript evaluation failed: %s\n", duk_safe_to_string(ctx, -1));
        ret = 1;
    }

    duk_pop(ctx);  /* pop eval result */

    duk_destroy_heap(ctx);

    return ret;
}

/**
 * @brief reads a JavaScript file
 * @param filename "<executable name>.js"
 * @param js the source as UTF-8 will be stored here
 * @return error code or 0
 */
static int readJS(wchar_t* filename, char** js)
{
    //wprintf(L"readJS()");

    *js = 0;

    int err = 0;

    HANDLE f = CreateFile(filename, GENERIC_READ,
            FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        err = 1;
        printError(GetLastError());
    }

    LARGE_INTEGER sz;
    if (!err) {
        if (!GetFileSizeEx(f, &sz)) {
            err = 1;
            printError(GetLastError());
        }
    }

    if (!err) {
        if (sz.HighPart > 0 || sz.LowPart > 100 * 1024 * 1024) {
            err = 1;
            wprintf(L"JavaScript file is too big\n");
        }
    }

    if (!err) {
        *js = malloc(sz.LowPart + 1);

        DWORD read;
        if (!ReadFile(f, *js, sz.LowPart, &read, NULL)) {
            err = 1;
            printError(GetLastError());
        } else {
            *(*js + read) = 0;
        }
    }

    if (err) {
        free(*js);
        *js = 0;
    }

    if (f != INVALID_HANDLE_VALUE)
        CloseHandle(f);

    return err;
}

#endif

BOOL fileExists(LPCTSTR szPath)
{
  DWORD dwAttrib = GetFileAttributes(szPath);

  return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
         !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

int wmain(int argc, wchar_t **argv)
{
    //wprintf(L"main()");

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

#ifdef EXE_PROXY_JAVASCRIPT
    // find the name of the .js file
    wchar_t* javaScript = 0;
    if (!ret) {
        javaScript = wcsdup(exe);
        _wcslwr(javaScript);
        int len = wcslen(javaScript);
        if (len > 4 && wcscmp(L".exe", javaScript + len - 4) == 0) {
            wchar_t* p = javaScript + len - 3;
            *p = 'j';
            p++;
            *p = 's';
            p++;
            *p = 0;
        } else {
            ret = ERROR_EXIT_CODE;
            wprintf(L"Program name does not end with .exe\n");
        }
    }

    if (!ret) {
        if (fileExists(javaScript)) {
            char* js = 0;
            //wprintf(L"Reading %s", javaScript);
            if (readJS(javaScript, &js) != 0) {
                ret = 1;
            }

            if (!ret) {
                char* exeUTF8 = toUTF8(exe);
                // wprintf(L"Executing %s", javaScript);
                if (executeJS(js, exeUTF8) != 0) {
                    ret = ERROR_EXIT_CODE;
                }
                free(exeUTF8);
            }
            free(js);
        }
    }

    free(javaScript);

    //wprintf(L"Done");

#else
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
        ret = exec(newExe, cmdLine);
    }

    free(cmdLine);
    free(newExe);
#endif

    free(exe);
    free(args);

    free(g_ServiceName);

    return ret;
}

