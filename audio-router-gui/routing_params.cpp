#include "routing_params.h"
#include <cstring>
#include <cassert>

size_t pointers_size(const global_routing_params* global)
{
    size_t size = 0;
    if(global->module_name_ptr)
        size += wcslen((wchar_t*)global->module_name_ptr) * sizeof(wchar_t) + sizeof(wchar_t);
    if(global->local.device_id_ptr)
        size += wcslen((wchar_t*)global->local.device_id_ptr) * sizeof(wchar_t) + sizeof(wchar_t);

    return size;
}

size_t global_size(const global_routing_params* global, bool struct_size)
{
    size_t size = sizeof(global_routing_params);
    if(!struct_size)
        size += pointers_size(global);
    if(global->next_global_ptr)
        size += global_size((global_routing_params*)global->next_global_ptr, struct_size);

    return size;
}

void free(global_routing_params* global)
{
    for(global_routing_params* next = global; next != NULL;)
    {
        const global_routing_params* old = next;
        next = (global_routing_params*)next->next_global_ptr;
        delete [] (wchar_t*)old->module_name_ptr;
        delete [] (wchar_t*)old->local.device_id_ptr;
        delete old;
    }
}

void serialize(const global_routing_params* global, unsigned char* memory)
{
    const size_t full_size = global_size(global);
    const size_t headers_size = global_size(global, true);

    uint64_t pointer = 0, names_pointer = headers_size;
    for(const global_routing_params* next = global;;next = (global_routing_params*)next->next_global_ptr)
    {
        global_routing_params* new_global = (global_routing_params*)(memory + (DWORD_PTR)pointer);

        new_global->version = next->version;
        new_global->local.session_guid_and_flag = next->local.session_guid_and_flag;
        new_global->local.pid = next->local.pid;
        {
            wchar_t* new_name;

            if(next->module_name_ptr)
            {
                new_name = (wchar_t*)(memory + (DWORD_PTR)names_pointer);
                new_global->module_name_ptr = names_pointer;
                wcscpy(new_name, (wchar_t*)next->module_name_ptr);
                names_pointer += wcslen(new_name) * sizeof(wchar_t) + sizeof(wchar_t);
            }
            else
                new_global->module_name_ptr = NULL;

            if(next->local.device_id_ptr)
            {
                new_name = (wchar_t*)(memory + (DWORD_PTR)names_pointer);
                new_global->local.device_id_ptr = names_pointer;
                wcscpy(new_name, (wchar_t*)next->local.device_id_ptr);
                names_pointer += wcslen(new_name) * sizeof(wchar_t) + sizeof(wchar_t);
            }
            else
                new_global->local.device_id_ptr = NULL;
        }

        pointer += sizeof(global_routing_params);
        if(next->next_global_ptr)
            new_global->next_global_ptr = pointer;
        else
        {
            new_global->next_global_ptr = NULL;
            break;
        }
    }

    assert(pointer == headers_size && names_pointer == full_size);
}