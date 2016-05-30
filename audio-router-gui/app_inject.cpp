#include "app_inject.h"
#include "wtl.h"
#include "policy_config.h"
#include "util.h"
#include "routing_params.h"
#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <cassert>

#define MAKE_SESSION_GUID_AND_FLAG(guid, flag) \
    ((((DWORD)flag) << (sizeof(DWORD) * 8 - 2)) | (((DWORD)guid) & ~(3 << (sizeof(DWORD) * 8 - 2))))
#define SESSION_GUID_BEGIN /*8*/5

DWORD app_inject::session_guid = 1 << SESSION_GUID_BEGIN;

DWORD app_inject::get_session_guid_and_flag(bool duplicate, bool saved_routing)
{
    return MAKE_SESSION_GUID_AND_FLAG(session_guid++, duplicate ? 2 : 1);
    //if(!saved_routing)
    //    return MAKE_SESSION_GUID_AND_FLAG(session_guid++, duplicate ? 2 : 1);
    //else
    //{
    //    const DWORD mod = (1 << 31) >> (SESSION_GUID_BEGIN + 2); // +2 because flags are included
    //    const DWORD guid = (DWORD)(rand() % mod) << (SESSION_GUID_BEGIN + 1);
    //    return MAKE_SESSION_GUID_AND_FLAG(guid, duplicate ? 2 : 1);
    //}
}

app_inject::app_inject()
{
}

void app_inject::clear_devices(devices_t& devices)
{
    for(size_t i = 0; i < devices.size(); i++)
    {
        if(devices[i] != NULL)
            devices[i]->Release();
    }
    devices.clear();
}

void app_inject::get_devices(devices_t& devices)
{
    clear_devices(devices);

    IMMDeviceEnumerator* pEnumerator;
    IMMDeviceCollection* pDevices;

    if(CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator) != S_OK)
    {
        SAFE_RELEASE(pEnumerator);
        return;
    }

    if(pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices) != S_OK)
    {
        SAFE_RELEASE(pDevices);
        pEnumerator->Release();
        return;
    }
    pEnumerator->Release();
    UINT count;
    if(pDevices->GetCount(&count) != S_OK)
    {
        pDevices->Release();
        return;
    }
    IMMDevice *pEndpoint = NULL;
    for(ULONG i = 0; i < count; i++)
    {
        pDevices->Item(i, &pEndpoint);
        devices.push_back(pEndpoint);
    }
    
    pDevices->Release();
}

void app_inject::populate_devicelist()
{
    this->device_names.clear();

    devices_t devices;
    this->get_devices(devices);

    IMMDevice* pEndpoint = NULL;
    for(size_t i = 0; i < devices.size(); i++)
    {
        IPropertyStore* pProps;
        LPWSTR pwszID;
        pEndpoint = devices[i];
        // Get the endpoint ID string.
        pEndpoint->GetId(&pwszID);
        pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
        PROPVARIANT varName;
        // Initialize container for property value.
        PropVariantInit(&varName);

        // Get the endpoint's friendly-name property.
        pProps->GetValue(PKEY_Device_FriendlyName, &varName);

        this->device_names.push_back(varName.pwszVal);

        CoTaskMemFree(pwszID);
        PropVariantClear(&varName);
        pProps->Release();
    }
    
    this->clear_devices(devices);
}

// createprocessw lpcommandline must not be const literal
void app_inject::inject(DWORD process_id, bool x86, size_t device_index, flush_t flush, bool duplicate)
{
    IMMDevice* pEndpoint = NULL;
    LPWSTR pwszID = NULL;
    global_routing_params routing_params;
    routing_params.version = 0;
    routing_params.module_name_ptr = routing_params.next_global_ptr = NULL;
    routing_params.local.pid = process_id;

    // set routing params
    if(device_index > 0)
    {
        devices_t devices;
        this->get_devices(devices);

        // initializes interprocess arguments for routing audio to new device
        pEndpoint = devices[device_index - 1];
        pEndpoint->GetId(&pwszID);

        this->clear_devices(devices);

        routing_params.local.session_guid_and_flag = get_session_guid_and_flag(duplicate);
        routing_params.local.device_id_ptr = (uint64_t)pwszID;
    }
    else
    {
        // initializes interprocess arguments for routing audio to default device
        // (acts as deloading the audio routing functionality)
        routing_params.local.session_guid_and_flag = 0; // unload dll flag
            //MAKE_SESSION_GUID_AND_FLAG(session_guid++, 0); // unload dll flag
        routing_params.local.device_id_ptr = NULL;
    }

    // create file mapped object for ipc
    security_attributes sec(FILE_MAP_ALL_ACCESS);
    CHandle hfile(CreateFileMapping(
        INVALID_HANDLE_VALUE, sec.get(), PAGE_READWRITE, 0, 
        global_size(&routing_params), 
        L"Local\\audio-router-file"));
    if(hfile == NULL || (pwszID && *pwszID == NULL))
    {
        CoTaskMemFree(pwszID);
        throw_errormessage(GetLastError());
    }

    unsigned char* buffer = (unsigned char*)MapViewOfFile(hfile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if(buffer == NULL)
    {
        CoTaskMemFree(pwszID);
        throw_errormessage(GetLastError());
    }

    serialize(&routing_params, buffer);

    UnmapViewOfFile(buffer);
    CoTaskMemFree(pwszID);

    if(pEndpoint != NULL || device_index == 0)
    {
        try
        {
            this->inject_dll(process_id, x86);
        }
        catch(std::wstring err)
        {
            throw err;
        }

        if(flush == SOFT)
            reset_all_devices(false);
        else if(flush == HARD && !reset_all_devices(true))
            throw std::wstring(L"Stream flush in target process failed.\n");

        return;
    }

    assert(false);
}

void app_inject::inject_dll(DWORD pid, bool x86, DWORD tid, DWORD flags)
{
    // flag = 0: audio router dll is explicitly loaded
    // flag = 1: bootstrapper and audio router dll are implicitly loaded
    // flag = 2: bootstrapper is implicitly loaded
    // flag = 3: bootstrapper is explicitly loaded
    assert(flags <= 3);
    assert((flags && flags <= 2) ? tid : true);
    assert(pid);

    // retrieve the paths
    WCHAR filepath[MAX_PATH] = {0};
    GetModuleFileName(NULL, filepath, MAX_PATH);
    CPath path(filepath);
    path.RemoveFileSpec();
    std::wstring folder = L"\"", exe = L"\"";
    folder += path;
    exe += path;
    exe += L"\\do";
    if(!x86)
        exe += L"64";
    folder += L"\"";
    exe += L".exe\"";

    // inject
    TCHAR buf[32] = {0};
    TCHAR buf2[32] = {0};
    TCHAR buf3[32] = {0};
    _itot((int)pid, buf, 10);
    _itot((int)tid, buf2, 10);
    _itot((int)flags, buf3, 10);

    std::wstring command_line = exe;
    command_line += L" ";
    command_line += buf;
    command_line += L" ";
    command_line += folder;
    command_line += L" ";
    command_line += buf2;
    command_line += L" ";
    command_line += buf3;
    
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

#ifdef _DEBUG
#define CREATEPROCESS_FLAGS CREATE_SUSPENDED
#define RESUME_THREAD() ResumeThread(pi.hThread);
#define DO_EXE_WAIT_TIMEOUT INFINITE
#else
#define CREATEPROCESS_FLAGS 0
#define RESUME_THREAD() 0;
#define DO_EXE_WAIT_TIMEOUT 5000
#endif

    if(!CreateProcess(
        NULL, const_cast<LPWSTR>(command_line.c_str()), 
        NULL, NULL, FALSE, CREATEPROCESS_FLAGS, NULL, NULL, &si, &pi))
        throw_errormessage(GetLastError());
    RESUME_THREAD()

    DWORD result = WaitForSingleObject(pi.hProcess, DO_EXE_WAIT_TIMEOUT);
    if(result == WAIT_OBJECT_0)
    {
        DWORD exitcode;
        GetExitCodeProcess(pi.hProcess, &exitcode);
        if(exitcode != 0)
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            throw_errormessage(exitcode);
        }
    }
    else
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        throw std::wstring(L"Audio Router delegate did not respond in time.\n");
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}