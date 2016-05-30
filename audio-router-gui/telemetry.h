#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>

class telemetry
{
private:
    static SOCKET try_connect(std::string&);
    static void try_send(
        SOCKET, const std::string& host, const std::string& path, const std::string& tm);
    static void xor(std::string&, const char* data, size_t);
    static void resource_op(LPCWSTR name, int&, bool write = false);
    static DWORD WINAPI on_open_threadproc(LPVOID);
    static DWORD WINAPI on_routing_threadproc(LPVOID);
public:
    static const char xor_key = 76;
    static const int max_count = 10, invalid_count = -1;
    static const char host_xor[], path_xor[], path_routed_xor[];
private:
    int times_routed, times_opened;
    bool triggered;

    void update_on_open();
public:
    telemetry();
    ~telemetry();

    void update_on_routing();
};