#include <Windows.h>
#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <string>
#include <cassert>
#include "main.h"
#include "patcher.h"
#include "patch.h"

#pragma comment(lib, "uuid.lib")

// TODO: unify constants and structures between dll and gui
#define ROUTING_MASK (((DWORD)0x3) << (sizeof(DWORD) * 8 - 2))
#define SESSION_MASK ((~((DWORD)0)) >> 2)
#define GET_ROUTING_FLAG(a) (a >> (sizeof(DWORD) * 8 - 2))

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) \
   if(x != NULL)        \
   {                    \
      x->Release();     \
      x = NULL;         \
   }
#endif

struct device_id_t
{
    LPWSTR device_id_str;
    device_id_t(LPWSTR device_id_str) : device_id_str(device_id_str) {}
    void Release() 
    {
        delete [] this->device_id_str;
        delete this;
    }
};

typedef duplicate<device_id_t> device_id_duplicate;
typedef HRESULT (__stdcall *activate_t)(IMMDevice*, REFIID, DWORD, PROPVARIANT*, void**);
HRESULT __stdcall activate_patch(IMMDevice*, REFIID, DWORD, PROPVARIANT*, void**);

bool apply_explicit_routing();

patcher<activate_t> patch_activate(activate_patch);
CRITICAL_SECTION CriticalSection;
DWORD session_flag;
device_id_duplicate* device_ids = NULL;

// TODO: streamline device id parameter applying

// FALSE ret val will decrement the ref count
// TRUE ret val will keep the dll
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if(fdwReason == DLL_PROCESS_ATTACH)
    {
        HRESULT hr;
        IMMDeviceEnumerator* pEnumerator;
        IMMDevice* pDevice;

        hr = CoInitialize(NULL);
        if(hr != S_OK)
            return FALSE;
        hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), NULL,
            CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
            (void**)&pEnumerator);
        if(hr != S_OK)
            return FALSE;
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if(hr != S_OK)
        {
            pEnumerator->Release();
            return FALSE;
        }

        DWORD* patch_activate_ptr = ((DWORD***)pDevice)[0][3];

        pDevice->Release();
        pEnumerator->Release();

        InitializeCriticalSectionAndSpinCount(&CriticalSection, 0x00000400);
        patch_activate.patch(patch_activate_ptr);

        // patch only after the unloading of the library cannot happen
        if(!apply_explicit_routing() && !apply_implicit_routing())
            // (patches are automatically reverted)
            return FALSE;
    }
    else if(fdwReason == DLL_THREAD_ATTACH)
        apply_explicit_routing();
    else if(fdwReason == DLL_PROCESS_DETACH)
    {
        // TODO: include critical sections in the patcher
        // TODO: decide if delete device id str(practically does not have any effect)
        // (patches are automatically reverted)
        DeleteCriticalSection(&CriticalSection);
    }

    return TRUE;
}

bool apply_parameters(const local_routing_params& params)
{
    const DWORD proc_id = GetCurrentProcessId();
    const DWORD target_id = params.pid;
    if(proc_id != target_id)
        return false;

    const DWORD session_flag = params.session_guid_and_flag;
    if((session_flag & ROUTING_MASK) == 0)
    {
        // critical section necessary in case if another thread is currently
        // performing initialization
        EnterCriticalSection(&CriticalSection);

        patch_activate.revert();
        delete device_ids;
        device_ids = NULL;

        LeaveCriticalSection(&CriticalSection);

        return true;
    }
    else if(params.device_id_ptr)
    {
        // critical section necessary in case if another thread is currently
        // performing initialization
        EnterCriticalSection(&CriticalSection);

        ::session_flag = session_flag;

        std::wstring device_id;
        if(GET_ROUTING_FLAG(session_flag) == 1)
        {
            delete device_ids;
            device_ids = NULL;
        }
        for(const wchar_t* p = (const wchar_t*)params.device_id_ptr; *p; p++)
            device_id += *p;
        WCHAR* device_id_str = new WCHAR[device_id.size() + 1];
        memcpy(device_id_str, device_id.c_str(), device_id.size() * sizeof(WCHAR));
        device_id_str[device_id.size()] = 0;
        if(device_ids == NULL)
            device_ids = new device_id_duplicate(new device_id_t(device_id_str));
        else
            device_ids->add(new device_id_t(device_id_str));
        patch_activate.apply();

        LeaveCriticalSection(&CriticalSection);

        return true;
    }

    return false;
}

bool apply_explicit_routing()
{
    CHandle hfile(OpenFileMappingW(FILE_MAP_READ, FALSE, L"Local\\audio-router-file"));
    if(hfile == NULL)
        return false;

    unsigned char* buffer = (unsigned char*)MapViewOfFile(hfile, FILE_MAP_COPY, 0, 0, 0);
    if(buffer == NULL)
        return false;

    global_routing_params* params = rebase(buffer);
    const bool ret = apply_parameters(params->local);
    UnmapViewOfFile(buffer);
    return ret;
}

HRESULT __stdcall activate_patch(
    IMMDevice* this_, REFIID iid, DWORD dwClsCtx, 
    PROPVARIANT* pActivationParams, void** ppInterface)
{
    EnterCriticalSection(&CriticalSection);
    patch_activate.revert();

    // use default since default audio device has been requested
    if(device_ids == NULL)
    {
        HRESULT hr = this_->Activate(iid, dwClsCtx, pActivationParams, ppInterface);
        // patch_activate.apply();
        LeaveCriticalSection(&CriticalSection);
        return hr;
    }

    //// 7ED4EE07-8E67-4CD4-8C1A-2B7A5987AD42
    //if(iid != __uuidof(IAudioClient))
    //{
    //    HRESULT hr = this_->Activate(iid, dwClsCtx, pActivationParams, ppInterface);
    //    patch_activate.apply();
    //    LeaveCriticalSection(&CriticalSection);
    //    return hr;
    //}

    {
        HRESULT hr;
        IMMDeviceEnumerator* pEnumerator = NULL;
        IMMDeviceCollection* pDevices = NULL;
        IMMDevice* pDevice = NULL;

        if((hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), NULL,
            CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator),
            (void**)&pEnumerator)) != S_OK)
        {
            SAFE_RELEASE(pEnumerator);
            patch_activate.apply();
            LeaveCriticalSection(&CriticalSection);
            return hr;
        }
        if((hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices)) != S_OK)
        {
            SAFE_RELEASE(pDevices);
            pEnumerator->Release();
            patch_activate.apply();
            LeaveCriticalSection(&CriticalSection);
            return hr;
        }
        UINT count;
        if((hr = pDevices->GetCount(&count)) != S_OK)
        {
            pDevices->Release();
            pEnumerator->Release();
            patch_activate.apply();
            LeaveCriticalSection(&CriticalSection);
            return hr;
        }

        LPWSTR dev_id = NULL;
        this_->GetId(&dev_id);
        bool endpoint_found = dev_id ? false : true;
        IMMDevice *pEndpoint = NULL;
        for(ULONG i = 0; i < count; i++)
        {
            LPWSTR pwszID;

            pDevices->Item(i, &pEndpoint);
            // Get the endpoint ID string.
            pEndpoint->GetId(&pwszID);
            if(!pDevice && wcscmp(device_ids->proxy->device_id_str, pwszID) == 0)
                pDevice = pEndpoint;
            // check if the this_ endpoint device is in render group
            if(!endpoint_found && wcscmp(pwszID, dev_id) == 0)
                endpoint_found = true;

            CoTaskMemFree(pwszID);
            if(!pDevice)
                pEndpoint->Release();
        }
        CoTaskMemFree(dev_id);

        if(!endpoint_found)
        {
            HRESULT hr = this_->Activate(iid, dwClsCtx, pActivationParams, ppInterface);
            if(pDevice)
                pDevice->Release();
            pDevices->Release();
            pEnumerator->Release();
            patch_activate.apply();
            LeaveCriticalSection(&CriticalSection);
            return hr;
        }
        // TODO: device id might be invalid in case if the endpoint has been disconnected
        /*if(cont)
            MessageBoxA(
            NULL, "Endpoint device was not found during routing process.",
            NULL, MB_ICONERROR);*/

        if(!pDevice ||
            (hr = pDevice->Activate(iid, dwClsCtx, pActivationParams, ppInterface)) != S_OK)
        {
            if(pDevice)
                pDevice->Release();
            else
                hr = AUDCLNT_E_DEVICE_INVALIDATED;
            pDevices->Release();
            pEnumerator->Release();
            patch_activate.apply();
            LeaveCriticalSection(&CriticalSection);
            return hr;
        }

        pDevice->Release();
        pDevices->Release();
        pEnumerator->Release();

        // (metro apps seem to use an undocumented interface)
        if(iid != __uuidof(IAudioClient))
        {
            // the interface is initialized for another device which allows for routing,
            // but duplication will not be available since the interface is unknown
            patch_activate.apply();
            LeaveCriticalSection(&CriticalSection);
            return hr;
        }

        GUID* guid = new GUID;
        ZeroMemory(guid, sizeof(GUID));
        const DWORD session_guid = session_flag & SESSION_MASK;
        size_t starting_byte = 7;
        memcpy(((char*)guid) + starting_byte, &session_guid, sizeof(session_guid));

        patch_iaudioclient((IAudioClient*)*ppInterface, guid);
    }

    // add additional devices
    for(device_id_duplicate* next = device_ids->next; next != NULL; next = next->next)
    {
        IMMDeviceEnumerator* pEnumerator = NULL;
        IMMDeviceCollection* pDevices = NULL;
        IMMDevice* pDevice = NULL;
        IAudioClient* pAudioClient = NULL;

        if(CoCreateInstance(
            __uuidof(MMDeviceEnumerator), NULL,
            CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator),
            (void**)&pEnumerator) != S_OK)
        {
            SAFE_RELEASE(pEnumerator);
            continue;
        }
        if(pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices) != S_OK)
        {
            SAFE_RELEASE(pDevices);
            pEnumerator->Release();
            continue;
        }
        UINT count;
        if(pDevices->GetCount(&count) != S_OK)
        {
            pDevices->Release();
            pEnumerator->Release();
            continue;
        }

        bool cont = true;
        IMMDevice *pEndpoint = NULL;
        for(ULONG i = 0; i < count && cont; i++)
        {
            LPWSTR pwszID;

            pDevices->Item(i, &pEndpoint);
            // Get the endpoint ID string.
            pEndpoint->GetId(&pwszID);
            if(wcscmp(next->proxy->device_id_str, pwszID) == 0)
                cont = false;

            CoTaskMemFree(pwszID);
            if(cont)
                pEndpoint->Release();
        }
        pDevice = pEndpoint;
        // TODO: device id might be invalid in case if the endpoint has been disconnected
        if(cont)
            MessageBoxA(
            NULL, "Endpoint device was not found during routing process.",
            NULL, MB_ICONERROR);

        if(!cont && 
            pDevice->Activate(iid, dwClsCtx, pActivationParams, (void**)&pAudioClient) != S_OK)
        {
            SAFE_RELEASE(pAudioClient);
            pDevice->Release();
            pDevices->Release();
            pEnumerator->Release();
            continue;
        }

        if(!cont)
            pDevice->Release();
        pDevices->Release();
        pEnumerator->Release();

        if(cont)
        {
            continue;
        }

        /*GUID* guid = new GUID;
        ZeroMemory(guid, sizeof(GUID));
        const DWORD session_guid = session_flag & SESSION_MASK;
        size_t starting_byte = 7;
        memcpy(((char*)guid) + starting_byte, &session_guid, sizeof(session_guid));*/

        get_duplicate((IAudioClient*)*ppInterface)->add(pAudioClient);
    }

    patch_activate.apply();
    LeaveCriticalSection(&CriticalSection);

    return S_OK;
}