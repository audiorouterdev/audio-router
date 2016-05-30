#pragma once

#include <mmdeviceapi.h>
#include <vector>
#include <string>

// TODO: standardize flags for inject dll
// TODO: change the order of parameters in inject dll

class app_inject
{
public:
    typedef std::vector<IMMDevice*> devices_t;
    enum flush_t {SOFT = 0, HARD, NONE};

    static void get_devices(devices_t&);
    static void clear_devices(devices_t&);
    // throws formatted last error message
    // TODO: use both as flag parameter
    static void inject_dll(DWORD id, bool x86, DWORD tid = 0, DWORD flags = 0);
    static DWORD get_session_guid_and_flag(bool duplicate, bool saved_routing = false);
private:
    static DWORD session_guid;
public:
    std::vector<std::wstring> device_names;

    app_inject();
    void populate_devicelist();
    // device_index 0 is reserved for default device;
    // throws wstring;
    // duplication ignored on device_index 0
    void inject(DWORD process_id, bool x86, size_t device_index, flush_t flush, bool duplicate = false);
};