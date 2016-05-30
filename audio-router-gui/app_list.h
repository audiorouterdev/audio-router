#pragma once

#include <Windows.h>
#include <vector>
#include <string>

class app_list
{
public:
    struct app_info
    {
        std::wstring name;
        DWORD id;
        bool x86;
    };
    typedef std::vector<app_info> apps_t;
    typedef std::vector<std::wstring> filters_t;
private:
    bool populate_list(bool x86, const filters_t&);
    static bool get_app_info(app_info&, const filters_t&, bool x86, bool query_name);
public:
    apps_t apps;

    bool populate_list();
    bool populate_list(const filters_t&);
    // OR filter
    static bool get_app_info(app_info&, const filters_t& = filters_t(), bool query_name = true);
};