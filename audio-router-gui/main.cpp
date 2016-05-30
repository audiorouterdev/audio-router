#include "window.h"
#include "util.h"
#include "telemetry.h"
#include <gdiplus.h>
#include <cassert>
//#include <time.h>
#include <stdlib.h>

#pragma comment(lib, "gdiplus.lib")

CAppModule _Module;
HANDLE audio_router_mutex;

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    /*srand(time(NULL));*/

    /*global_routing_params* params = new global_routing_params;
    params->version = 0;
    params->local.module_name_ptr = (uint64_t)new wchar_t[wcslen(L"full\\path\\to\\module.dll") + 1];
    wcscpy((wchar_t*)params->local.module_name_ptr, L"full\\path\\to\\module.dll");
    params->local.device_id_ptr = (uint64_t)new wchar_t[wcslen(L"device id") + 1];
    wcscpy((wchar_t*)params->local.device_id_ptr, L"device id");

    global_routing_params* params2 = new global_routing_params;
    params2->version = 0;
    params2->local.module_name_ptr = (uint64_t)new wchar_t[wcslen(L"full\\path\\to\\module.dll") + 1];
    wcscpy((wchar_t*)params2->local.module_name_ptr, L"full\\path\\to\\module.dll");
    params2->local.device_id_ptr = (uint64_t)new wchar_t[wcslen(L"device id") + 1];
    wcscpy((wchar_t*)params2->local.device_id_ptr, L"device id");
    params->next_global_ptr = (uint64_t)params2;

    global_routing_params* params3 = new global_routing_params;
    params3->version = 0;
    params3->local.module_name_ptr = (uint64_t)new wchar_t[wcslen(L"full\\path\\to\\module.dll") + 1];
    wcscpy((wchar_t*)params3->local.module_name_ptr, L"full\\path\\to\\module.dll");
    params3->local.device_id_ptr = NULL;
    params2->next_global_ptr = (uint64_t)params3;
    params3->next_global_ptr = NULL;

    unsigned char* serialized = serialize(params);
    free(params);
    global_routing_params* serialized_params = rebase(serialized);

    MessageBox(NULL, (wchar_t*)serialized_params->local.module_name_ptr, NULL, 0);
    MessageBox(NULL, (wchar_t*)((global_routing_params*)serialized_params->next_global_ptr)->local.device_id_ptr, NULL, 0);

    delete [] serialized_params;*/

    if(GetModuleHandle(L"Audio Router.exe") == NULL)
    {
        MessageBox(
            NULL, L"Wrong application name. Audio Router will close.", NULL, MB_ICONERROR);
        return 0;
    }

    {
        security_attributes sec(GENERIC_ALL, security_attributes::DEFAULT);
        assert(sec.get() != NULL);
        audio_router_mutex = CreateMutex(sec.get(), FALSE, L"Local\\audio-router-mutex");
    }
    if(audio_router_mutex == NULL)
    {
        MessageBox(
            NULL, L"Mutex creation failed. Audio Router will close.", NULL, MB_ICONERROR);
        return 0;
    }
    else if(GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(audio_router_mutex);
        MessageBox(
            NULL, L"Another instance of Audio Router is already running. " \
            L"Audio Router will close.", NULL, MB_ICONERROR);
        return 0;
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if(hr != S_OK)
    {
        CloseHandle(audio_router_mutex);
        MessageBox(
            NULL, L"COM could not be initialized. Audio Router will close.", NULL, MB_ICONERROR);
        return 0;
    }

    if(_Module.Init(NULL, hInstance) != S_OK)
    {
        CoUninitialize();
        CloseHandle(audio_router_mutex);
        MessageBox(
            NULL, L"ATL could not be initialized. Audio Router will close.", NULL, MB_ICONERROR);
        return 0;
    }

    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    if(Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Gdiplus::Ok)
    {
        _Module.Term();
        CoUninitialize();
        CloseHandle(audio_router_mutex);
        MessageBox(
            NULL, L"GDI+ could not be initialized. Audio Router will close.", NULL, MB_ICONERROR);
        return 0;
    }

    MSG msg = {0};
    std::unique_ptr<bootstrapper> bootstrap;
    try
    {
        // TODO: decide if create a dummy bootstapper in case if the initialization fails
        bootstrap.reset(new bootstrapper);
    }
    catch(std::wstring err)
    {
        err += L"Audio Router will close.";
        MessageBox(NULL, err.c_str(), NULL, MB_ICONERROR);
        goto cleanup;
    }
    {
        window win(bootstrap.get());
        RECT r = {CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT + WIN_WIDTH, CW_USEDEFAULT + WIN_HEIGHT};
        if(win.CreateEx(NULL, &r) == NULL)
        {
            MessageBox(NULL, L"Could not create window. Audio Router will close.", NULL, MB_ICONERROR);
            goto cleanup;
        }
        win.ShowWindow(nCmdShow);
        win.UpdateWindow();

        while(GetMessage(&msg, NULL, 0, 0) > 0)
        {
            if(win.dlg_main && IsDialogMessage(*win.dlg_main, &msg))
                continue;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

cleanup:
    Gdiplus::GdiplusShutdown(gdiplusToken);
    _Module.Term();
    CoUninitialize(); // this thread should be the last one to call uninitialize
    CloseHandle(audio_router_mutex);

    return (int)msg.wParam;
}