#include <Windows.h>
#include <AclAPI.h>
#include "..\audio-router\patcher.h"
#include "..\audio-router-gui\routing_params.h"
#define DELEGATION_ONLY_NAME
#include "..\audio-router-gui\delegation.h"
#include "CHandle.h"
#include <new>

#define XOR_KEY ((char)(0xea))
#define XOR_KEY2 ((char)(0x14))

#ifndef _WIN64
#define DLL_NAME L"bootstrapper.dll"
#else
#define DLL_NAME L"bootstrapper64.dll"
#endif

//
// Unicode strings are counted 16-bit character strings. If they are
// NULL terminated, Length does not include trailing NULL.
//

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
#ifdef MIDL_PASS
    [size_is(MaximumLength / 2), length_is((Length) / 2) ] USHORT * Buffer;
#else // MIDL_PASS
    _Field_size_bytes_part_(MaximumLength, Length) PWCH   Buffer;
#endif // MIDL_PASS
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;
typedef const UNICODE_STRING *PCUNICODE_STRING;

// https://msdn.microsoft.com/en-us/library/windows/desktop/aa813741(v=vs.85).aspx
typedef struct _RTL_USER_PROCESS_PARAMETERS {
  BYTE           Reserved1[16];
  PVOID          Reserved2[10];
  UNICODE_STRING ImagePathName;
  UNICODE_STRING CommandLine;
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;

typedef LPVOID POBJECT_ATTRIBUTES, PPROCESS_CREATE_INFO, PPROCESS_ATTRIBUTE_LIST;
typedef patcher<NTSTATUS (NTAPI*)(
    PHANDLE ProcessHandle,
    PHANDLE ThreadHandle,
    ACCESS_MASK ProcessDesiredAccess,
    ACCESS_MASK ThreadDesiredAccess,
    POBJECT_ATTRIBUTES ProcessObjectAttributes,
    POBJECT_ATTRIBUTES ThreadObjectAttributes,
    ULONG ProcessFlags,
    ULONG ThreadFlags,
    PRTL_USER_PROCESS_PARAMETERS ProcessParameters,
    PPROCESS_CREATE_INFO CreateInfo,
    PPROCESS_ATTRIBUTE_LIST AttributeList)> ntcreateuserprocess_patcher_t;

bool try_init_bootstrapper(ntcreateuserprocess_patcher_t*);
//bool try_deinit_bootstrapper();
NTSTATUS NTAPI ntcreateuserprocess_patch(PHANDLE,
    PHANDLE,
    ACCESS_MASK,
    ACCESS_MASK,
    POBJECT_ATTRIBUTES,
    POBJECT_ATTRIBUTES,
    ULONG,
    ULONG,
    PRTL_USER_PROCESS_PARAMETERS,
    PPROCESS_CREATE_INFO,
    PPROCESS_ATTRIBUTE_LIST);

ntcreateuserprocess_patcher_t* patch_ntcreateuserprocess;
HANDLE hheap;
bool is_patched;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if(fdwReason == DLL_PROCESS_ATTACH)
    {
        is_patched = false;

        if(GetModuleHandle(L"Audio Router.exe") != NULL)
            return FALSE;

        hheap = HeapCreate(0, 0, 0);
        patch_ntcreateuserprocess = (ntcreateuserprocess_patcher_t*)
            HeapAlloc(hheap, 0, sizeof(ntcreateuserprocess_patcher_t));
        new (patch_ntcreateuserprocess) ntcreateuserprocess_patcher_t(ntcreateuserprocess_patch);

        if(!try_init_bootstrapper(patch_ntcreateuserprocess))
            return FALSE;
    }
    else if(fdwReason == DLL_THREAD_ATTACH)
    {
        // restore patch if it was reverted when audio router was closed
        // (only if audio router is present again)
        CHandle hfile(OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\audio-router-mutex"));
        if(hfile && !is_patched)
            try_init_bootstrapper(patch_ntcreateuserprocess);
        else if(!hfile && is_patched)
        {
            patch_ntcreateuserprocess->lock();
            patch_ntcreateuserprocess->revert();
            is_patched = false;
            patch_ntcreateuserprocess->unlock();
        }
    }
    else if(fdwReason == DLL_PROCESS_DETACH)
    {
        // TODO: remove
        patch_ntcreateuserprocess->lock();
        patch_ntcreateuserprocess->revert();
        is_patched = false;
        patch_ntcreateuserprocess->unlock();
    }

    return TRUE;
}

NTSTATUS NTAPI ntcreateuserprocess_patch(
    PHANDLE ProcessHandle,
    PHANDLE ThreadHandle,
    ACCESS_MASK ProcessDesiredAccess,
    ACCESS_MASK ThreadDesiredAccess,
    POBJECT_ATTRIBUTES ProcessObjectAttributes,
    POBJECT_ATTRIBUTES ThreadObjectAttributes,
    ULONG ProcessFlags,
    ULONG ThreadFlags,
    PRTL_USER_PROCESS_PARAMETERS ProcessParameters,
    PPROCESS_CREATE_INFO CreateInfo,
    PPROCESS_ATTRIBUTE_LIST AttributeList)
{
    // https://blogs.msdn.microsoft.com/winsdk/2009/11/10/access-denied-on-a-mutex/
    // https://blogs.msdn.microsoft.com/winsdk/2010/05/31/dealing-with-administrator-and-standard-users-context/

    // in the context of any other than the app information process(elevation handler)
    // the integrity level will be same or less

    // TODO: handle unicode string properly
    bool in_list = false;
    CHandle hfile(OpenFileMappingW(FILE_MAP_READ | READ_CONTROL, FALSE, L"Local\\audio-router-file-startup"));
    CHandle hfile2(OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\audio-router-mutex"));
    LPCWSTR lpApplicationName = ProcessParameters->ImagePathName.Buffer;
    ntcreateuserprocess_patcher_t::func_t real = (ntcreateuserprocess_patcher_t::func_t)
        patch_ntcreateuserprocess->get_function();

    bool wrong_creator = false;
    // check if the fmo is created with an administrators token
    if(hfile)
    {
        PSID owner;
        PSECURITY_DESCRIPTOR desc = NULL;
        DWORD errcode = GetSecurityInfo(hfile, SE_KERNEL_OBJECT, OWNER_SECURITY_INFORMATION,
            &owner, NULL, NULL, NULL, &desc);
        if(errcode != ERROR_SUCCESS || !IsWellKnownSid(owner, WinBuiltinAdministratorsSid))
            wrong_creator = true;
        LocalFree(desc);
    }
    if(wrong_creator || !hfile2)
    {
        // remove the patch
        patch_ntcreateuserprocess->lock();
        patch_ntcreateuserprocess->revert();
        is_patched = false;
        patch_ntcreateuserprocess->unlock();
        return real(ProcessHandle, ThreadHandle, ProcessDesiredAccess,
            ThreadDesiredAccess, ProcessObjectAttributes, ThreadObjectAttributes,
            ProcessFlags, ThreadFlags, ProcessParameters, CreateInfo, AttributeList);
    }

    // check if application is in the startup list
    if(hfile && lpApplicationName)
    {
        unsigned char* view_of_file = (unsigned char*)MapViewOfFile(hfile, FILE_MAP_COPY, 0, 0, 0);
        global_routing_params* params;
        if(view_of_file)
        {
            for(params = rebase(view_of_file);
                params != NULL; 
                params = (global_routing_params*)params->next_global_ptr)
            {
                if(params->module_name_ptr && 
                    lstrcmpi((LPWSTR)params->module_name_ptr, lpApplicationName) == 0)
                {
                    in_list = true;
                    break;
                }
            }
            UnmapViewOfFile(view_of_file);
        }
    }

    // try connecting to audio router's pipe for do.exe delegation
    DWORD last_err = 0;
    const DWORD interval = 1, max_time = 50;
    CHandle hpipe;
    for(DWORD time = 0; time < max_time; time += interval)
    {
        HANDLE p = CreateFile(
            PIPE_NAME,
            GENERIC_READ | GENERIC_WRITE,
            0, // no sharing
            NULL,
            OPEN_EXISTING,
            0,
            NULL);
        if(p != INVALID_HANDLE_VALUE)
        {
            hpipe.Attach(p);
            break;
        }
        if(GetLastError() != ERROR_PIPE_BUSY || !WaitNamedPipe(PIPE_NAME, interval))
        {
            last_err = GetLastError();
            break;
        }
    }

    // TODO: copy code of ntcreateuserprocess and call there(guarantees no race conditions)
    // TODO: make this process faster by omitting do exe delegation in certain cases

    // revert holds lock until apply;
    // TODO: reverting process is atomic
    patch_ntcreateuserprocess->lock();
    patch_ntcreateuserprocess->revert();

    NTSTATUS status = real(ProcessHandle, ThreadHandle, ProcessDesiredAccess,
        ThreadDesiredAccess, ProcessObjectAttributes, ThreadObjectAttributes,
        ProcessFlags, ThreadFlags, ProcessParameters, CreateInfo, AttributeList);

    patch_ntcreateuserprocess->apply();
    patch_ntcreateuserprocess->unlock();

    // TODO: this might add a lot of overhead to createprocess routine
    if(status == 0 && hpipe && ProcessHandle && ThreadHandle)
    {
        DWORD pid = GetProcessId(*ProcessHandle), tid = GetThreadId(*ThreadHandle);
        if(pid && tid)
        {
            // delegate the bootstrapping process to do exe via audio router
            DWORD mode = PIPE_READMODE_MESSAGE;
            SetNamedPipeHandleState(hpipe, &mode, NULL, NULL);
            // flag 2 indicates that bootstrapping is only injected
            DWORD pid_tid_both[3] = {pid, tid, (DWORD)(in_list ? 1 : 2) };
            DWORD exitcode, bytes_read;
            BOOL success = TransactNamedPipe(
                hpipe, pid_tid_both, sizeof(pid_tid_both), &exitcode, sizeof(exitcode),
                &bytes_read, NULL);
            assert(success);
            assert(GetLastError() != ERROR_MORE_DATA);
            assert(exitcode == 0);
        }
    }

    return status;
}

bool try_init_bootstrapper(ntcreateuserprocess_patcher_t* ntcreateuserprocess_patch)
{
    CHandle hfile(OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\audio-router-mutex"));
    if(hfile != NULL)
    {
        char ntdll[] = {
            'n' ^ XOR_KEY, 't' ^ ~XOR_KEY, 'd' ^ XOR_KEY, 'l' ^ ~XOR_KEY, 'l' ^ XOR_KEY,
            '.' ^ ~XOR_KEY, 'd' ^ XOR_KEY, 'l' ^ ~XOR_KEY, 'l' ^ XOR_KEY, 0 ^ ~XOR_KEY};
        ntdll[sizeof(ntdll) - 1] ^= ~XOR_KEY;
        char* i;
        bool b = false;
        for(i = ntdll; *i; i++, b = !b)
            *i ^= b ? ~XOR_KEY : XOR_KEY;
        char ntcreateuserprocess[] = {
            'N' ^ XOR_KEY2, 't' ^ ~XOR_KEY2, 'C' ^ XOR_KEY2, 'r' ^ ~XOR_KEY2, 'e' ^ XOR_KEY2, 
            'a' ^ ~XOR_KEY2, 't' ^ XOR_KEY2, 'e' ^ ~XOR_KEY2, 'U' ^ XOR_KEY2, 's' ^ ~XOR_KEY2, 
            'e' ^ XOR_KEY2, 'r' ^ ~XOR_KEY2, 'P' ^ XOR_KEY2, 'r' ^ ~XOR_KEY2, 'o' ^ XOR_KEY2, 
            'c' ^ ~XOR_KEY2, 'e' ^ XOR_KEY2, 's' ^ ~XOR_KEY2, 's' ^ XOR_KEY2, 0 ^ ~XOR_KEY2};
        ntcreateuserprocess[sizeof(ntcreateuserprocess) - 1] ^= ~XOR_KEY2;
        b = false;
        for(i = ntcreateuserprocess; *i; i++, b = !b)
            *i ^= b ? ~XOR_KEY2 : XOR_KEY2;

        // initialize the bootstrapping functionality
        void* proc_addr;
        proc_addr = GetProcAddress(GetModuleHandleA(ntdll), ntcreateuserprocess);
        ntcreateuserprocess_patch->lock();
        const int ret = ntcreateuserprocess_patch->patch(proc_addr);
        assert(ret == 0);
        if(ret == 0)
            is_patched = true;
        ntcreateuserprocess_patch->unlock();
        return true;
    }
    return false;
}

//bool try_deinit_bootstrapper()
//{
//    // bootstrapper can be unloaded from process only when debugging,
//    // that's because the unloading process might be unsafe
//#ifdef _DEBUG
//    CHandle hfile(OpenFileMappingW(FILE_MAP_READ, FALSE, L"Local\\audio-router-file-startup"));
//    if(hfile == NULL)
//    {
//        HMODULE module = GetModuleHandle(DLL_NAME);
//        assert(module != NULL);
//        FreeLibraryAndExitThread(module, 0);
//        return true;
//    }
//    return false;
//#else
//    return true;
//#endif
//}