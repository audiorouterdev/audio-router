#include "main.h"
//#include "..\audio-router-gui\app_inject.h"
#include <string>

// TODO: standardization of error codes between app inject and do

#ifndef _WIN64

#define ADDR_1 7
#define ADDR_2 24

unsigned char blob[] =
{
    0x55,
    0x8b, 0xec,
    0xff, 0x75, 0x08,
    0xb8, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xd0,
    0x85, 0xc0,
    0x74, 0x06,
    0x33, 0xc0,
    0x5d,
    0xc2, 0x04, 0x00,
    0xb8, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xd0,
    0x5d,
    0xc2, 0x04, 0x00
};

#else

#define ADDR_1 6
#define ADDR_2 34

unsigned char blob[] =
{
    0x48, 0x83, 0xec, 0x28,
    0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xd0,
    0x48, 0x85, 0xc0,
    0x74, 0x07,
    0x33, 0xc0,
    0x48, 0x83, 0xc4, 0x28,
    0xc3,
    0x48, 0x83, 0xc4, 0x28,
    0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xe0
};

#endif

const size_t blob_size = sizeof(blob);

DWORD bootstrap(DWORD pid, DWORD tid, const std::wstring& folder, bool both);

int CALLBACK WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    int argc = 0;
    LPTSTR* argv = CommandLineToArgvW(GetCommandLine(), &argc);
    if(argc != 5)
        return CUSTOM_ERR(0);

    // TODO: refactor folder parameter since it is present in argv[0]
    // localfree not issued on argv on purpose
    LPTSTR strpid = argv[1];
    std::wstring dll = argv[2]; // folder
    LPTSTR strtid = argv[3];
    LPTSTR strboth = argv[4];

    DWORD pid = wcstoul(strpid, NULL, 10), tid = wcstoul(strtid, NULL, 10);
    DWORD flags = wcstoul(strboth, NULL, 10);
    if(pid == 0 || pid == ULONG_MAX || tid == ULONG_MAX || flags > 3)
        return CUSTOM_ERR(1);

    // check if perform bootstrapping instead
    if(flags && flags <= 2)
        return ::bootstrap(pid, tid, dll, flags == 1 ? true : false);

    dll += L"\\";
    if(flags == 3)
        dll += BOOTSTRAPPER_DLL_NAME;
    else
        dll += AUDIO_ROUTER_DLL_NAME;
    const size_t dll_len_size = dll.length() * sizeof(wchar_t) + sizeof(wchar_t);

    // perform actual injection
    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if(process == NULL)
        return GetLastError();

    LPVOID addr = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW");
    LPVOID addr2 = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetLastError");
    if(addr == NULL || addr2 == NULL)
    {
        const DWORD errcode = GetLastError();
        CloseHandle(process);
        return errcode;
    }
    const LPVOID arg = (LPVOID)VirtualAllocEx(
        process, NULL, dll_len_size + blob_size,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if(arg == NULL)
    {
        const DWORD errcode = GetLastError();
        CloseHandle(process);
        return errcode;
    }
    if(!WriteProcessMemory(process, arg, dll.c_str(), dll_len_size, NULL))
    {
        const DWORD errcode = GetLastError();
        VirtualFreeEx(process, arg, 0, MEM_RELEASE);
        CloseHandle(process);
        return errcode;
    }

    // add binary blob to report error case in the remote thread
    const DWORD_PTR load_library_addr = (DWORD_PTR)addr;
    memcpy(blob + ADDR_1, &load_library_addr, sizeof(load_library_addr));
    const DWORD_PTR get_last_error_addr = (DWORD_PTR)addr2;
    memcpy(blob + ADDR_2, &get_last_error_addr, sizeof(get_last_error_addr));
    if(!WriteProcessMemory(process, (char*)arg + dll_len_size, blob, blob_size, NULL))
    {
        const DWORD errcode = GetLastError();
        VirtualFreeEx(process, arg, 0, MEM_RELEASE);
        CloseHandle(process);
        return errcode;
    }
    DWORD old_;
    if(!VirtualProtectEx(process, (char*)arg + dll_len_size, blob_size, PAGE_EXECUTE_READWRITE, &old_))
    {
        const DWORD errcode = GetLastError();
        VirtualFreeEx(process, arg, 0, MEM_RELEASE);
        CloseHandle(process);
        return errcode;
    }
    addr = (char*)arg + dll_len_size;

    HANDLE threadID = CreateRemoteThread(
        process, NULL, 0, (LPTHREAD_START_ROUTINE)addr, arg, NULL, NULL);
    if(threadID == NULL)
    {
        const DWORD errcode = GetLastError();
        VirtualFreeEx(process, arg, 0, MEM_RELEASE);
        CloseHandle(process);
        return errcode;
    }

    DWORD result = WaitForSingleObject(threadID, 4000);
    if(result == WAIT_OBJECT_0)
    {
        VirtualFreeEx(process, arg, 0, MEM_RELEASE);

        DWORD exitcode;
        GetExitCodeThread(threadID, &exitcode);
        if(exitcode != 0)
        {
            CloseHandle(threadID);
            CloseHandle(process);
            // exitcode will hold the getlasterror value from the remote thread
            return exitcode;
        }
    }
    else
    {
        // releasing memory not safe because target process might finish initialization later
        CloseHandle(threadID);
        CloseHandle(process);
        return CUSTOM_ERR(3);
    }

    CloseHandle(threadID);
    CloseHandle(process);

    return 0;
}