#pragma once

#include <Windows.h>
#include <stdint.h>

struct local_routing_params
{
    DWORD pid;
    DWORD session_guid_and_flag;
    uint64_t device_id_ptr;
};

struct global_routing_params
{
    BYTE version;
    uint64_t module_name_ptr;
    local_routing_params local;
    uint64_t next_global_ptr;
};

void free(global_routing_params*);
void serialize(const global_routing_params*, unsigned char* buffer);
size_t global_size(const global_routing_params* global, bool struct_size = false);

static global_routing_params* rebase(unsigned char* blob)
{
    for(global_routing_params* next = (global_routing_params*)blob;
        next != NULL;
        next = (global_routing_params*)next->next_global_ptr)
    {
        if(next->module_name_ptr)
            next->module_name_ptr += (DWORD_PTR)blob;
        if(next->local.device_id_ptr)
            next->local.device_id_ptr += (DWORD_PTR)blob;
        if(next->next_global_ptr)
            next->next_global_ptr += (DWORD_PTR)blob;
    }

    return (global_routing_params*)blob;
}