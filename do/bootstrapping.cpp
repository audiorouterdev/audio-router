#include "main.h"
#include "..\audio-router-gui\wtl.h"
#include <string>
#include <stdint.h>

#pragma pack(push, 1)

template<typename Address>
struct jmp_to {};

template<>
struct jmp_to<uint32_t>
{
    typedef uint32_t address_t; 

    const unsigned char mov_ax = 0xb8;
    address_t addr;
    const WORD jmp_ax = 0xe0ff;
};

template<>
struct jmp_to<uint64_t>
{
    typedef uint64_t address_t; 

    const WORD mov_ax = 0xb848;
    address_t addr;
    const WORD jmp_ax = 0xe0ff;
};

template<typename Address>
struct call {};

template<>
struct call<uint32_t>
{
    typedef uint32_t address_t; 

    const unsigned char mov_ax = 0xb8;
    address_t addr;
    const WORD call_ax = 0xd0ff;
};

template<>
struct call<uint64_t>
{
    typedef uint64_t address_t; 

    const WORD mov_ax = 0xb848;
    address_t addr;
    const WORD call_ax = 0xd0ff;
};

template<typename Address>
struct shellcode_thunk {};

template<>
struct shellcode_thunk<uint32_t>
{
    typedef uint32_t address_t; 

    /*const BYTE dbg_break = 0xcc;*/

    const BYTE mov_bx = 0xbb;
    address_t load_library_offset;
    const BYTE mov_cx = 0xb9;
    address_t bootstrapper_str_addr;
    const BYTE mov_dx = 0xba;
    address_t audiorouter_str_addr;
    call<address_t> call_shellcode;
    jmp_to<address_t> jmp_real_entrypoint;
};

template<>
struct shellcode_thunk<uint64_t>
{
    typedef uint64_t address_t;

    /*const BYTE dbg_break = 0xcc;*/

    const WORD mov_bx = 0xbb48;
    address_t load_library_offset;
    const WORD mov_cx = 0xb948;
    address_t bootstrapper_str_addr;
    const WORD mov_dx = 0xba48;
    address_t audiorouter_str_addr;
    call<address_t> call_shellcode;
    jmp_to<address_t> jmp_real_entrypoint;
};

// http://www.ragestorm.net/blogs/?p=369
// http://mcdermottcybersecurity.com/articles/windows-x64-shellcode

#ifndef _WIN64

const BYTE shellcode[] = {
    0x6A, 0x30, 0x5E, 0x64, 0xAD, 0x8B, 0x70, 0x0C, 0x8B, 0x76, 0x1C, 0x8B, 0x46, 
    0x08, 0x80, 0x7E, 0x1C, 0x18, 0x8B, 0x36, 0x75, 0xF5, 0x01, 0xD8, 0x52, 0x50, 
    0x51, 0xFF, 0xD0, 0x58, 0xFF, 0xD0, 0xC3
};

#else

const BYTE shellcode[] = {
    0x6A, 0x60, 0x5E, 0x65, 0x48, 0xAD, 0x48, 0x8B, 0x70, 0x18, 0x48, 0x8B, 0x76,
    0x30, 0x48, 0x8B, 0x46, 0x10, 0x80, 0x7E, 0x38, 0x18, 0x48, 0x8B, 0x36, 0x75,
    0xF3, 0x48, 0x01, 0xD8, 0x52, 0x50, 0x48, 0x83, 0xEC, 0x20, 0xFF, 0xD0, 0x48,
    0x83, 0xC4, 0x20, 0x58, 0x59, 0x48, 0x83, 0xEC, 0x20, 0xFF, 0xD0, 0x48, 0x83,
    0xC4, 0x20, 0xC3
};

#endif

#pragma pack(pop)

typedef shellcode_thunk<size_t> shellcode_thunk_t;
typedef jmp_to<size_t> jmp_to_t;

DWORD bootstrap(DWORD pid, DWORD tid, const std::wstring& folder, bool both)
{
    CHandle hprocess(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid));
    CHandle hthread(OpenThread(THREAD_ALL_ACCESS, FALSE, tid));
    if(hprocess == NULL || hthread == NULL)
        return GetLastError();

    // http://reverseengineering.stackexchange.com/questions/8086/changing-start-address-of-thread-using-combination-of-get-and-setthreadcontext
    // http://www.exploit-monday.com/2012/04/64-bit-process-replacement-in.html
    // get & set entrypoint
    CONTEXT ctx;
    LPVOID entrypoint;
    ctx.ContextFlags = CONTEXT_INTEGER;
    GetThreadContext(hthread, &ctx);
#ifdef _WIN64
#define CTX_EAX ctx.Rcx
#else
#define CTX_EAX ctx.Eax
#endif
    entrypoint = (LPVOID)CTX_EAX;

    // calculate loadlibrary offset
    HMODULE kernel32 = GetModuleHandle(L"kernel32.dll");
    LPVOID loadlibrary = (LPVOID)GetProcAddress(kernel32, "LoadLibraryW");
    if(!loadlibrary)
        return GetLastError();
    const size_t loadlibrary_addr_offset = (size_t)loadlibrary - (size_t)kernel32;

    // allocate space
    std::wstring bootstrapper_dll = folder + L"\\", audiorouter_dll = folder + L"\\";
    bootstrapper_dll += BOOTSTRAPPER_DLL_NAME;
    audiorouter_dll += AUDIO_ROUTER_DLL_NAME;
    const size_t bootstrapper_dll_len_size = bootstrapper_dll.length() * sizeof(wchar_t) + sizeof(wchar_t),
        audiorouter_dll_len_size = audiorouter_dll.length() * sizeof(wchar_t) + sizeof(wchar_t);
    
    LPVOID blob = VirtualAllocEx(
        hprocess, NULL, bootstrapper_dll_len_size + audiorouter_dll_len_size +
        sizeof(shellcode_thunk_t) + sizeof(shellcode),
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if(!blob)
        return GetLastError();

    // initialize code
    CTX_EAX = (DWORD_PTR)((char*)blob + bootstrapper_dll_len_size + audiorouter_dll_len_size);
    shellcode_thunk_t thunk;
    thunk.jmp_real_entrypoint.addr = (shellcode_thunk_t::address_t)entrypoint;
    thunk.bootstrapper_str_addr = (shellcode_thunk_t::address_t)blob;
    if(both)
        thunk.audiorouter_str_addr = (shellcode_thunk_t::address_t)
        ((char*)blob + bootstrapper_dll_len_size);
    else
        thunk.audiorouter_str_addr = NULL;
    thunk.load_library_offset = loadlibrary_addr_offset;
    thunk.call_shellcode.addr = (shellcode_thunk_t::address_t)((char*)blob + 
        bootstrapper_dll_len_size + audiorouter_dll_len_size + sizeof(shellcode_thunk_t));

    // write
    if(!WriteProcessMemory(hprocess, blob, bootstrapper_dll.c_str(), bootstrapper_dll_len_size, NULL))
        return GetLastError();
    if(!WriteProcessMemory(hprocess, (char*)blob + bootstrapper_dll_len_size, audiorouter_dll.c_str(),
        audiorouter_dll_len_size, NULL))
        return GetLastError();
    if(!WriteProcessMemory(hprocess, (char*)blob + bootstrapper_dll_len_size + 
        audiorouter_dll_len_size, &thunk, sizeof(shellcode_thunk_t), NULL))
        return GetLastError();
    if(!WriteProcessMemory(hprocess, (char*)blob + bootstrapper_dll_len_size + 
        audiorouter_dll_len_size + sizeof(shellcode_thunk_t), shellcode, sizeof(shellcode), NULL))
        return GetLastError();

    SetThreadContext(hthread, &ctx);

    return 0;
}