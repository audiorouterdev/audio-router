#include <initguid.h>
#include "policy_config.h"
#include "app_inject.h"
#include <LM.h>
#include <cassert>

#pragma comment(lib, "netapi32.lib")

extern HANDLE audio_router_mutex;

HRESULT SetDefaultAudioPlaybackDevice(LPCWSTR devID)
{
    IPolicyConfigVista *pPolicyConfig;
    ERole reserved = eConsole;

    HRESULT hr = CoCreateInstance(
                    __uuidof(CPolicyConfigVistaClient),
                    NULL, 
                    CLSCTX_ALL, 
                    __uuidof(IPolicyConfigVista), 
                    (LPVOID *)&pPolicyConfig);

    if (SUCCEEDED(hr))
    {
        hr = pPolicyConfig->SetDefaultEndpoint(devID, reserved);
        pPolicyConfig->Release();
    }

    return hr;
}

template<typename T, typename U>
bool reset_all_device_formats()
{
    bool ret = true;
    T* pPolicyConfig;
    HRESULT hr = CoCreateInstance(
                    __uuidof(U),
                    NULL, 
                    CLSCTX_ALL, 
                    __uuidof(T), 
                    (LPVOID *)&pPolicyConfig);

    if(SUCCEEDED(hr))
    {
        app_inject::devices_t devices;
        app_inject::get_devices(devices);

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
            if(pProps->GetValue(PKEY_AudioEngine_DeviceFormat, &varName) == S_OK)
                ret &= (bool)pPolicyConfig->SetDeviceFormat(pwszID, 
                (WAVEFORMATEX*)varName.blob.pBlobData, NULL);

            CoTaskMemFree(pwszID);
            PropVariantClear(&varName);
            pProps->Release();
        }
        if(devices.empty())
            ret = false;
    
        app_inject::clear_devices(devices);

        pPolicyConfig->Release();
    }

    return !ret;
}

bool iswin10orgreater()
{
    LPSERVER_INFO_101 pBuf = NULL;
    if(NetServerGetInfo(NULL, 101, (LPBYTE*)&pBuf) == NERR_Success)
    {
        const bool win10 = (pBuf->sv101_version_major & MAJOR_VERSION_MASK) >= 10;
        NetApiBufferFree(pBuf);
        return win10;
    }

    return true;
}

bool reset_all_devices(bool hard_reset)
{
    DWORD res = WaitForSingleObject(audio_router_mutex, INFINITE);
    assert(res == WAIT_OBJECT_0);

    bool ret = false;
    if(hard_reset)
    {
        // check for win10 and use vista interface
        if(iswin10orgreater())
            ret = reset_all_device_formats<IPolicyConfigVista, CPolicyConfigVistaClient>();
        else
            ret = reset_all_device_formats<IPolicyConfig, CPolicyConfigClient>();
    }
    else
    {
        IMMDeviceEnumerator* pEnumerator;
        IMMDevice* pDevice;
        CoCreateInstance(
            __uuidof(MMDeviceEnumerator), NULL,
            CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator),
            (void**)&pEnumerator);
        pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        LPWSTR pwszID;
        pDevice->GetId(&pwszID);
        SetDefaultAudioPlaybackDevice(pwszID);
        CoTaskMemFree(pwszID);
        pDevice->Release();
        pEnumerator->Release();

        ret = true;
    }

    ReleaseMutex(audio_router_mutex);
    return ret;
}