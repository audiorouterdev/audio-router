#pragma once

#include <Windows.h>
#include <string>
#include <memory>
#include "wtl.h"
#include "routing_params.h"
#include "delegation.h"

struct global_routing_params;
struct IMMDevice;

class bootstrapper
{
private:
    CHandle local_file, implicit_params;
    global_routing_params* routing_params;
    bool available;
    HWND hwnd;
    // service for audio router dll to invoke do exe
    std::unique_ptr<delegation> delegator;

    static void create_default_params(global_routing_params&);
    // throws
    void load_local_implicit_params(bool create_default_if_empty = true);
    // throws
    void update_save();
public:
    bool is_saved_routing(DWORD pid, IMMDevice* dev) const;
    bool is_managed_app(DWORD pid) const;
    // throws
    void save_routing(DWORD pid, IMMDevice* dev);
    // throws
    bool delete_all(DWORD pid);

    explicit bootstrapper(HWND = NULL);
    ~bootstrapper();
};