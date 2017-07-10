#include "app_list.h"
#include < Psapi.h > 
#include < algorithm > 

bool app_list::populate_list(bool x86, const filters_t & filters) {
    DWORD processes[1024], needed;
    if (!EnumProcesses(processes, sizeof(processes), &needed))
        return false;

    size_t count = needed / sizeof(DWORD);
    for (DWORD i = 0; i < count; i++) {
        app_info info;
        info.id = processes[i];
        if (this->get_app_info(info, filters, x86, true))
            this->apps.push_back(info);
    }

    return true;
}

bool app_list::populate_list() {
    filters_t filters;
    filters.push_back(L"mmdevapi.dll");
    return this->populate_list(filters);
}

bool app_list::populate_list(const filters_t & filters) {
    this->apps.clear();

    if (!this->populate_list(true, filters))
        return false;
#ifdef _WIN64
    return this->populate_list(false, filters);
#else
    return true;
#endif
}

bool app_list::get_app_info(app_info & info, const filters_t & filters, bool x86, bool query_name) {
    HMODULE hmodules[1024];
    DWORD needed;
    HANDLE hprocess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, info.id);
    if (hprocess == NULL)
        return false;

#ifdef _WIN64
    BOOL px86;
    if (!IsWow64Process(hprocess, &px86)) {
        CloseHandle(hprocess);
        return false;
    }
    info.x86 = px86;
#else
    info.x86 = true;
#endif

    if (query_name && EnumProcessModulesEx(
        hprocess, hmodules, sizeof(hmodules), &needed, x86 ? LIST_MODULES_32BIT : LIST_MODULES_64BIT)) {
        WCHAR name[MAX_PATH] = { 0 };
        std::wstring exename;

        size_t name_size = sizeof(name) / sizeof(WCHAR);
        if (GetModuleBaseName(hprocess, hmodules[0], name, name_size)) {
            _wcslwr(name);
            exename = name;

            // skip filter
            if (filters.empty()) {
                info.name = exename;
                CloseHandle(hprocess);
                return true;
            }

            size_t needed_count = needed / sizeof(HMODULE);
            for (DWORD j = 0; j < needed_count; j++) {
                WCHAR name[MAX_PATH] = { 0 };
                if (GetModuleBaseName(hprocess, hmodules[j], name, name_size)) {
                    _wcslwr(name);
                    for (auto it = filters.begin(); it != filters.end(); it++)
                        if (*it == name) {
                            info.name = exename;
                            CloseHandle(hprocess);
                            return true;
                        };
                }
            }
        }
    }

    CloseHandle(hprocess);
    return !query_name;
}

bool app_list::get_app_info(app_info & info, const filters_t & filters, bool query_name) {
    if (get_app_info(info, filters, true, query_name))
        return true;
#ifdef _WIN64
    return get_app_info(info, filters, false, query_name);
#else
    return false;
#endif
}