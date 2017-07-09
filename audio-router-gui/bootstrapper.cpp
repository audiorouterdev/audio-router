#include "bootstrapper.h"
#include "app_list.h"
#include "app_inject.h"
#include "util.h"
#include <cassert>
#include <Psapi.h>

#define LOCAL_PARAMS_FILE L"saved_routings.dat"
#ifdef _DEBUG
#define SET_FILTERS() {filters.push_back(L"explorer.exe"); /*filters.push_back(L"steam.exe");*/}
#else
#define SET_FILTERS() {filters.push_back(L"explorer.exe"); filters.push_back(L"steam.exe");}
#endif
#define VERSION 0

// TODO: in future, audio router adds all apps that create sessions to a list
// in a hashed form
// TODO: a list that shows all apps that will be automatically routed

// TODO: in future, apps that route audio are prompted to restart
// so audio router can manage the stream;
// the stream won't appear in the default device list when it's managed

bool get_path(DWORD pid, WCHAR* path, DWORD size)
{
    CHandle hproc(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid));
    if(!hproc)
        return false;

    if(GetModuleFileNameEx(hproc, NULL, path, size) == 0)
        return false;

    return true;
}

void bootstrapper::create_default_params(global_routing_params& routing_params)
{
    routing_params.version = VERSION;
    routing_params.module_name_ptr = NULL;
    routing_params.local.pid = 0;
    routing_params.local.device_id_ptr = NULL;
    routing_params.local.session_guid_and_flag = 0;
    routing_params.next_global_ptr = NULL;
}

void try_loading(HANDLE hfile, global_routing_params*& params, LARGE_INTEGER size)
{
    if(params != NULL)
        delete [] (unsigned char*)params;
    params = NULL;

    unsigned char* view_of_file = (unsigned char*)MapViewOfFile(hfile, FILE_MAP_READ, 0, 0, 0);
    unsigned char* buffer = NULL;
    __try
    {
        buffer = new unsigned char[size.QuadPart];
        memcpy(buffer, view_of_file, size.QuadPart);
        UnmapViewOfFile(view_of_file);
        view_of_file = NULL;

        const size_t routing_params_size = global_size(params = rebase(buffer));
        if(size.QuadPart != routing_params_size || !routing_params_size) // validates parameters
            throw; // will be catched at the parent except clause
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        if(view_of_file)
            UnmapViewOfFile(view_of_file);
        if(buffer)
            delete [] buffer;
        params = NULL;
        throw std::wstring(L"The " LOCAL_PARAMS_FILE L" file is corrupted. Please delete the " \
            L"file and open Audio Router again.\n");
    }
}

void bootstrapper::load_local_implicit_params(bool create_default_if_empty)
{
    this->local_file.Attach(CreateFile(LOCAL_PARAMS_FILE, GENERIC_READ | GENERIC_WRITE, 
        FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
    if(this->local_file == INVALID_HANDLE_VALUE)
        throw std::wstring(L"Could not open or create a new file in the working directory.\n");

    LARGE_INTEGER size = {0};
    size.QuadPart = 1;
    GetFileSizeEx(this->local_file, &size);

    security_attributes sec(FILE_MAP_ALL_ACCESS);
    this->implicit_params.Attach(CreateFileMapping(this->local_file, sec.get(),
        PAGE_READONLY, 0, 0, L"Local\\audio-router-file-startup"));

    // try to create empty parameters structure in the file if the file was empty
    if(create_default_if_empty && this->implicit_params == NULL && size.QuadPart == 0)
    {
        this->update_save();
        this->load_local_implicit_params(false);
    }
    else if(this->implicit_params == NULL)
        throw std::wstring(L"The " LOCAL_PARAMS_FILE L" file is empty.\n");
    else
        // rebase and validate the view
        try_loading(this->implicit_params, this->routing_params, size);
}

bool bootstrapper::is_saved_routing(DWORD pid, IMMDevice* dev) const
{
    if(!dev)
        return false;

    LPWSTR id = NULL;
    if(dev)
        dev->GetId(&id);

    WCHAR path[MAX_PATH] = {0};
    if(!get_path(pid, path, MAX_PATH))
    {
        CoTaskMemFree(id);
        return false;
    }
    
    for(global_routing_params* params = this->routing_params;
        id && params != NULL;
        params = (global_routing_params*)params->next_global_ptr)
    {
        if(params->module_name_ptr && params->local.device_id_ptr &&
            lstrcmpi((LPWSTR)params->module_name_ptr, path) == 0 &&
            wcscmp((LPWSTR)params->local.device_id_ptr, id) == 0)
        {
            CoTaskMemFree(id);
            return true;
        }
    }

    CoTaskMemFree(id);
    return false;
}

bool bootstrapper::is_managed_app(DWORD pid) const
{
    WCHAR path[MAX_PATH] = {0};
    if(!get_path(pid, path, MAX_PATH))
        return false;
    
    for(global_routing_params* params = this->routing_params;
        params != NULL;
        params = (global_routing_params*)params->next_global_ptr)
    {
        if(params->module_name_ptr && params->local.device_id_ptr &&
            lstrcmpi((LPWSTR)params->module_name_ptr, path) == 0)
            return true;
    }

    return false;
}

void bootstrapper::save_routing(DWORD pid, IMMDevice* dev)
{
    if(!dev)
        throw std::wstring(L"Saving routing to Default Device is not supported.");

    if(!this->routing_params)
        return;
    WCHAR path[MAX_PATH] = {0};
    if(!get_path(pid, path, MAX_PATH))
        return;

    LPWSTR id = NULL;
    if(dev)
        dev->GetId(&id);
    // TODO: saved routing session guid might conflict with explicit session guid
    global_routing_params new_routing_params;
    new_routing_params.version = VERSION;
    new_routing_params.module_name_ptr = (uint64_t)path;
    new_routing_params.local.pid = 0;
    new_routing_params.local.device_id_ptr = (uint64_t)id;
    if(id)
        new_routing_params.local.session_guid_and_flag = 
        app_inject::get_session_guid_and_flag(true, true);
    else
        new_routing_params.local.session_guid_and_flag = 0;
    new_routing_params.next_global_ptr = NULL;

    // make last item of saved params to point to new params
    global_routing_params** params = &this->routing_params;
    for(;*params != NULL; params = (global_routing_params**)&((*params)->next_global_ptr));
    *params = &new_routing_params;

    // serialize params
    try
    {
        this->update_save();
    }
    catch(std::wstring err)
    {
        *params = NULL;
        CoTaskMemFree(id);
        throw err;
    }

    CoTaskMemFree(id);
    // TODO: file mapping should not signal whether implicit arguments are available or not
    this->load_local_implicit_params(false);
}

void bootstrapper::update_save()
{
    global_routing_params* routing_params = this->routing_params;
    global_routing_params default_params;
    create_default_params(default_params);
    if(!routing_params)
        routing_params = &default_params;

    // serialize params
    const size_t routing_params_size = global_size(routing_params);
    assert(routing_params_size > 0);
    if(routing_params_size == 0)
        throw std::wstring(L"The internal state of saved routings is corrupted.\n");

    // update the file and create a view
    this->implicit_params.Close();
    this->local_file.Close();

    // TODO: do not loop infinitely
    do
    {
        this->local_file = CHandle(NULL);
        this->local_file.Attach(CreateFile(LOCAL_PARAMS_FILE, GENERIC_READ | GENERIC_WRITE, 
            0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
    }
    while(this->local_file == INVALID_HANDLE_VALUE && GetLastError() == ERROR_USER_MAPPED_FILE);
    if(this->local_file == INVALID_HANDLE_VALUE)
    {
        throw std::wstring(L"Could not open file " LOCAL_PARAMS_FILE L" while saving routing. " \
            L"Audio Router is left in an invalid state and must be restarted.\n");
    }
    this->implicit_params.Attach(CreateFileMapping(this->local_file, NULL,
        PAGE_READWRITE, 0, (DWORD)routing_params_size, NULL));
    if(!this->implicit_params)
    {
        this->local_file.Close();
        throw std::wstring(L"Unexpected error occured. Old routed savings data is lost. " \
            L"Audio Router is left in an invalid state and must be restarted.\n");
    }
    unsigned char* view_of_file = (unsigned char*)MapViewOfFile(this->implicit_params, 
            FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if(!view_of_file)
    {
        this->implicit_params.Close();
        this->local_file.Close();
        throw std::wstring(L"Unexpected error occured while mapping. Old routed savings data " \
            L"is lost. Audio Router is left in an invalid state and must be restarted.\n");
    }
    serialize(routing_params, view_of_file);
    FlushViewOfFile(view_of_file, 0);
    UnmapViewOfFile(view_of_file);

    this->implicit_params.Close();
    this->local_file.Close();
}

bool bootstrapper::delete_all(DWORD pid)
{
    WCHAR path[MAX_PATH] = {0};
    if(!get_path(pid, path, MAX_PATH))
        return false;

    global_routing_params* const old_routing_params = this->routing_params;
    bool ret = false;

loop:
    for(global_routing_params* params = this->routing_params, *params_previous = NULL;
        params != NULL;
        params = (global_routing_params*)params->next_global_ptr)
    {
        if(params->module_name_ptr &&
            lstrcmpi((LPWSTR)params->module_name_ptr, path) == 0)
        {
            ret = true;
            if(!params_previous)
            {
                this->routing_params = (global_routing_params*)params->next_global_ptr;
                goto loop;
            }
            else
            {
                params_previous->next_global_ptr = params->next_global_ptr;
                goto loop;
            }
        }

        if(!params_previous)
            params_previous = this->routing_params;
        else
            params_previous = (global_routing_params*)params_previous->next_global_ptr;
    }

    if(ret)
    {
        this->update_save();
        this->routing_params = old_routing_params;
        this->load_local_implicit_params(false);
    }

    return ret;
}

bootstrapper::bootstrapper(HWND hwnd) : 
    available(false), hwnd(hwnd), routing_params(NULL)
{
    this->load_local_implicit_params();
    this->delegator.reset(new delegation);

    // TODO: explorer exe should be identified with path
    app_list list;
    app_list::filters_t filters;
    SET_FILTERS()
    list.populate_list(filters);
    for(auto it = list.apps.begin(); it != list.apps.end(); it++)
    {
        try
        {
            app_inject::inject_dll(it->id, it->x86, 0, 3);
            this->available = true;
        }
        catch(std::wstring err) {}
    }
    if(!this->available)
        MessageBox(this->hwnd, L"Initialization failed, Automatic routing functionality not available.", NULL, MB_ICONERROR);
}

bootstrapper::~bootstrapper()
{
    if(this->routing_params)
        delete [] (unsigned char*)this->routing_params;

    if(this->available)
    {
        // TODO: explorer exe should be identified with path
        app_list list;
        app_list::filters_t filters;
        SET_FILTERS()
        list.populate_list(filters);
        for(auto it = list.apps.begin(); it != list.apps.end(); it++)
        {
            try
            {
                app_inject::inject_dll(it->id, it->x86, 0, 3);
            }
            catch(std::wstring err) 
            {
#ifdef _DEBUG
                MessageBox(NULL, L"bootstrapper couldn't be notified", NULL, MB_ICONERROR);
#endif
            }
        }
    }
}